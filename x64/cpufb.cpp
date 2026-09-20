#include "table.hpp"
#include "thread_pool.hpp"
#include "cli.hpp"
#include <compute.hpp>
#include <load.hpp>
#include <memory_bandwidth.hpp>
#include <frequency.hpp>
#include <common.hpp>
#include <multiple_issue.hpp>
#include <cache_bandwidth.hpp>
#include <cache_topology.hpp>
#include "runtime_features.h"

#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <utility>

#if (defined(_AMX_INT8_) || defined(_AMX_BF16_)) && defined(__linux__)
#include <asm/prctl.h>
#include <sys/syscall.h>
#endif

#if defined(_AMX_INT8_) || defined(_AMX_BF16_)
#define _AMX_TILE_
#endif

using namespace std;
using namespace cpufb::cli;
extern vector<double> freq;
static struct CacheData cache_size;

// The warm-up pass runs 1/kWarmupLoopDivisor of the measured loop count.
static constexpr int64_t kWarmupLoopDivisor = 32;

#ifdef _AMX_TILE_
struct
{
    uint8_t palette_id;
    uint8_t start_row;
    uint8_t reserved_0[14];
    uint16_t colsb[16];
    uint8_t rows[16];
} __tilecfg;

void init_tile_cfg()
{
    int i;
    __tilecfg.palette_id = 1;
    __tilecfg.start_row = 0;
    for (i = 0; i < 14; i++) {
        __tilecfg.reserved_0[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        __tilecfg.colsb[i] = 64;
        __tilecfg.rows[i] = 16;
    }
    for (; i < 16; i++) {
        __tilecfg.colsb[i] = 0;
        __tilecfg.rows[i] = 0;
    }
}

static bool request_tile_data_permission()
{
#ifdef __linux__
#ifndef ARCH_REQ_XCOMP_PERM
#define ARCH_REQ_XCOMP_PERM 0x1023
#endif
#ifndef XFEATURE_XTILEDATA
#define XFEATURE_XTILEDATA 18
#endif
    return syscall(SYS_arch_prctl, ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA) ==
        0;
#else
    return false;
#endif
}
#endif

typedef void (*CacheKernel)(float *, int, int64_t);

struct cpubm_t
{
    std::string isa;
    std::string type;
    std::string dim;
    int64_t loop_time;
    int64_t comp_pl; // Mathematical/element operations per outer asm loop.
    int64_t inst_pl; // Benchmarked instructions per outer asm loop.
    void *params;
    void (*bench)(int64_t, void *);
    BenchmarkKind kind;
    // Load and multi-issue rows: the kernel itself, and for load rows the
    // cache level (1 or 2) whose capacity sizes the workset.
    CacheKernel cache_kernel;
    int cache_level;
    int bytes_per_load; // Bytes moved by one load instruction of the kernel.
#ifdef __linux__
    std::atomic<uint64_t> *cycle_count;
    std::atomic<int> *cycle_samples;
#endif
};
struct cache_bm_t
{
    float *cache_data;
    int inner_loop;
    int loop_time;
    void (*bench)(float *, int, int64_t);
#ifdef __linux__
    std::atomic<uint64_t> *cycle_count;
    std::atomic<int> *cycle_samples;
#endif
};
static vector<cpubm_t> bm_list;

static BenchmarkCatalog build_benchmark_catalog()
{
    BenchmarkCatalog catalog;
    catalog.reserve(bm_list.size());
    for (const cpubm_t &item : bm_list)
        catalog.push_back(BenchmarkInfo(item.isa, item.type, item.dim));
    pair_benchmark_latencies(catalog);
    return catalog;
}

struct ComputeResult
{
    double perf;
    double ipc;
    bool hardware_cycles;
};

static void reg_new_isa(std::string isa, std::string type, std::string dim,
    int64_t loop_time, int64_t comp_pl, void *params,
    void (*bench)(int64_t, void *), int64_t inst_pl = 16)
{
    cpubm_t new_one;
    new_one.isa = isa;
    new_one.type = type;
    new_one.dim = dim;
    new_one.loop_time = loop_time;
    new_one.comp_pl = comp_pl;
    new_one.inst_pl = inst_pl;
    new_one.params = params;
    new_one.bench = bench;
    new_one.kind = benchmark_kind_from_metric(dim);
    new_one.cache_kernel = nullptr;
    new_one.cache_level = 0;
    new_one.bytes_per_load = 0;
#ifdef __linux__
    new_one.cycle_count = nullptr;
    new_one.cycle_samples = nullptr;
#endif

    bm_list.push_back(new_one);
}

// An L1/L2-resident load row.  label is "L1 Cache"/"L2 Cache" for the first
// row of a level and "--------" for the rest; the level is explicit.
static void reg_load_kernel(const std::string &label, const std::string &type,
    int cache_level, CacheKernel kernel, int bytes_per_load)
{
    reg_new_isa(label, type, "Byte/Cycle", 0x186A00LL, 0, nullptr, nullptr);
    bm_list.back().cache_kernel = kernel;
    bm_list.back().cache_level = cache_level;
    bm_list.back().bytes_per_load = bytes_per_load;
}

// instructions_per_loop counts the inner loop including its loop control.
static void reg_multi_issue(const std::string &name, const std::string &type,
    int64_t instructions_per_loop, CacheKernel kernel)
{
    reg_new_isa(name, type, "IPC", 0x40000LL, 0, nullptr, nullptr,
        instructions_per_loop);
    bm_list.back().cache_kernel = kernel;
}

static void warn_estimated_cycles_once()
{
    static bool warned = false;
    if (warned) return;
    warned = true;
    cerr << "Warning: per-kernel perf_event cycle counts are unavailable; "
         << "IPC and latency are estimated from elapsed time x "
         << cpu_freq_counter_source()
         << " frequency. Lower /proc/sys/kernel/perf_event_paranoid or run "
         << "with CAP_PERFMON for counted cycles." << endl;
}

static void thread_func(void *params)
{
    cpubm_t *bm = (cpubm_t *)params;
#ifdef __linux__
    if (bm->cycle_count != nullptr && bm->cycle_samples != nullptr) {
        PerfEventCycle cycle_counter(0, false);
        if (cycle_counter.available()) {
            cycle_counter.start();
            bm->bench(bm->loop_time, bm->params);
            cycle_counter.stop();
            const long long cycles = cycle_counter.get_cycle();
            if (cycles > 0) {
                bm->cycle_count->fetch_add(
                    static_cast<uint64_t>(cycles), std::memory_order_relaxed);
                bm->cycle_samples->fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
    }
#endif
    if (bm->params) {
        bm->bench(bm->loop_time, bm->params);
    } else {
        bm->bench(bm->loop_time, nullptr);
    }
}
static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t *)params;
#ifdef __linux__
    if (bm->cycle_count != nullptr && bm->cycle_samples != nullptr) {
        PerfEventCycle cycle_counter(0, false);
        if (cycle_counter.available()) {
            cycle_counter.start();
            bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
            cycle_counter.stop();
            const long long cycles = cycle_counter.get_cycle();
            if (cycles > 0) {
                bm->cycle_count->fetch_add(
                    static_cast<uint64_t>(cycles), std::memory_order_relaxed);
                bm->cycle_samples->fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
    }
#endif
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    struct timespec start, end;
    cpubm_t warmup = item;
    warmup.loop_time = max<int64_t>(1, item.loop_time / kWarmupLoopDivisor);
    tpool_run_all(tm, thread_func, (void *)&warmup, &start, &end);

    ComputeResult result = {0.0, 0.0, false};
    cpubm_t measured_item = item;
#ifdef __linux__
    std::atomic<uint64_t> cycle_count(0);
    std::atomic<int> cycle_samples(0);
    measured_item.cycle_count = &cycle_count;
    measured_item.cycle_samples = &cycle_samples;
#endif
    if (!tpool_run_all(tm, thread_func, (void *)&measured_item, &start, &end))
        return result;
    double time_used = get_time(&start, &end);
    if (time_used <= 0.0) return result;
    result.perf = item.loop_time * item.comp_pl * tm->thread_num / time_used;
#ifdef __linux__
    const uint64_t cycles = cycle_count.load(std::memory_order_relaxed);
    if (cycles > 0 &&
        cycle_samples.load(std::memory_order_relaxed) == tm->thread_num) {
        result.ipc = static_cast<double>(item.loop_time) * item.inst_pl *
            tm->thread_num / cycles;
        result.hardware_cycles = true;
    }
#endif
    // perf_event_open is denied by default on many distributions
    // (perf_event_paranoid >= 3), containers and VMs.  Rather than dropping
    // every IPC and latency value, normalize by the calibrated cycle rate from
    // the frequency probe; its source is printed in the frequency table.
    if (!result.hardware_cycles && !freq.empty() && freq[0] > 0.0) {
        result.ipc = static_cast<double>(item.loop_time) * item.inst_pl /
            (time_used * freq[0] * 1e9);
        warn_estimated_cycles_once();
    }
    return result;
}

static string format_latency_cycles(double latency)
{
    stringstream ss;
    ss << fixed << setprecision(2) << latency;
    return ss.str();
}

static double cpubm_x64_latency(tpool_t *tm, cpubm_t &item)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    return result.ipc > 0.0 ? 1.0 / result.ipc : 0.0;
}

static void cpubm_x64_one(
    tpool_t *tm, cpubm_t &item, double latency, Table &table)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_perf_value(result.perf, item.dim);
    cont[3] = result.ipc > 0.0 ? to_string(result.ipc) : "-";
    cont[4] = latency > 0.0 ? format_latency_cycles(latency) : "-";
    table.addOneItem(cont);
}

static string format_reported_value(int value, const char *unit)
{
    return value > 0 ? to_string(value) + unit : "-";
}

// Capacity used to size the L1/L2 load worksets: same policy as ARM64, the
// OS topology when exposed and the empirical probe otherwise.
static int load_capacity_kb(int reported, int measured)
{
    return reported > 0 ? reported : measured;
}

// Formats value with 5 significant digits and a unit, or "-" when unknown.
static string format_or_dash(double value, const string &unit)
{
    if (value <= 0.0) return "-";
    stringstream text;
    text << setprecision(5) << value << unit;
    return text.str();
}

// One row of the L1/L2 load table; the method is in cache_bandwidth.hpp.
static void cpubm_x64_load(tpool_t *tm, cpubm_t &item, Table &table)
{
    // Level capacities: the OS topology when exposed, the probe otherwise.
    const size_t l1_bytes = 1024 *
        static_cast<size_t>(
            load_capacity_kb(cache_size.theory_L1, cache_size.test_L1));
    const size_t l2_bytes = 1024 *
        static_cast<size_t>(
            load_capacity_kb(cache_size.theory_L2, cache_size.test_L2));
    const bool is_l1 = item.cache_level == 1;
    const size_t workset = is_l1
        ? cpufb::cache_level_workset(0, l1_bytes)
        : cpufb::cache_level_workset(l1_bytes, l2_bytes);

    // Every x86 load kernel reads 512 bytes per count and per loop body.
    cpufb::LoadKernel kernel;
    kernel.name = item.type;
    kernel.function = item.cache_kernel;
    kernel.bytes_per_count = 512;
    kernel.block_bytes = 512;
    kernel.bytes_per_load = item.bytes_per_load;
    const double clock_hz = freq.empty() ? 0.0 : freq[0] * 1e9;
    const cpufb::CacheBandwidth bandwidth =
        cpufb::measure_cache_bandwidth(kernel, workset, tm, clock_hz);
    if (bandwidth.cycle_source == "time x clock") warn_estimated_cycles_once();

    vector<string> cont(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_or_dash(bandwidth.bytes_per_cycle, " " + item.dim);
    cont[3] = format_reported_value(
        is_l1 ? cache_size.theory_L1 : cache_size.theory_L2, " KB");
    cont[4] = format_reported_value(
        is_l1 ? cache_size.test_L1 : cache_size.test_L2, " KB");
    cont[5] = format_or_dash(bandwidth.gb_per_second, " GB/s");
    cont[6] = to_string(bandwidth.workset_bytes / 1024) + " KB";
    cont[7] = format_or_dash(bandwidth.load_ipc, "");
    cont[8] = to_string(item.bytes_per_load) + " B";
    table.addOneItem(cont);
}

static bool cpubm_x64_cache(
    std::vector<int> &set_of_threads, Table &table, const CliOptions &options)
{
    vector<string> cont;
    cont.resize(table.getCol());
    get_cacheline(&cache_size, set_of_threads[0]);
    get_multiway(&cache_size, set_of_threads[0]);
    get_cachesize(&cache_size, set_of_threads[0]);
    // The probe column always shows what was measured.  A result that
    // disagrees with the OS topology is flagged, never replaced, so the
    // report cannot agree with the reported value by construction.
    cont[3].clear();
    cont[4].clear();
    cont[0] = "L1 ways of associativity";
    cont[1] = format_reported_value(cache_size.theory_way, "");
    cont[2] = format_reported_value(cache_size.test_way, "");
    cont[5] = cpufb::describe_probe_agreement(
        cache_size.theory_way, cache_size.test_way, 1.0);
    table.addOneItem(cont);
    cont[0] = "cacheline size";
    cont[1] = format_reported_value(cache_size.theory_cacheline, " B");
    cont[2] = format_reported_value(cache_size.test_cacheline, " B");
    cont[5] = cpufb::describe_probe_agreement(
        cache_size.theory_cacheline, cache_size.test_cacheline, 1.0);
    table.addOneItem(cont);
    cont[0] = "L1 cache size";
    cont[1] = format_reported_value(cache_size.theory_L1, " KB");
    cont[2] = format_reported_value(cache_size.test_L1, " KB");
    cont[5] = cpufb::describe_probe_agreement(
        cache_size.theory_L1, cache_size.test_L1, 1.3);
    table.addOneItem(cont);
    cont[0] = "L2 cache size";
    cont[1] = format_reported_value(cache_size.theory_L2, " KB");
    cont[2] = format_reported_value(cache_size.test_L2, " KB");
    cont[5] = cpufb::describe_probe_agreement(
        cache_size.theory_L2, cache_size.test_L2, 1.3);
    table.addOneItem(cont);
    const cpufb::CacheLevelInfo l3 =
        cpufb::detect_data_cache_level(set_of_threads[0], 3);
    // Always shown: "none" is a result too, and it is how a machine without
    // an L3 is told apart from one whose L3 the OS merely does not report.
    cont[0] = "L3/unified cache capacity";
    cont[1] = l3.bytes > 0 ? cpufb::format_cache_capacity(l3.bytes) : "-";
    cont[2] = cpufb::format_probed_l3(
        cache_size.test_L3, cache_size.hierarchy_complete);
    cont[3].clear();
    cont[4].clear();
    cont[5] = cache_size.test_L3 > 0
        ? cpufb::describe_probe_agreement(
              l3.bytes / 1024.0, cache_size.test_L3, 1.3)
        : l3.source;
    table.addOneItem(cont);
    return append_cache_memory_bandwidth(options, table);
}

static void cpubm_x64_multiple_issue(tpool_t *tm, cpubm_t &item, Table &table)
{
    struct timespec start, end;
    cache_bm_t bm;
    const size_t size = 1024;
    // 64-byte alignment keeps the 512-byte zmm window from splitting lines.
    void *allocation = nullptr;
    if (posix_memalign(&allocation, 64, size) != 0) return;
    float *cache_data = static_cast<float *>(allocation);
    //Preventing Compiler Optimization
    for (size_t i = 0; i < size / sizeof(float); i++) {
        cache_data[i] = i;
    }
    int inner_loop = 1024;
    bm.bench = item.cache_kernel;
    bm.cache_data = cache_data;
    bm.inner_loop = inner_loop;
    bm.loop_time = item.loop_time;
#ifdef __linux__
    bm.cycle_count = nullptr;
    bm.cycle_samples = nullptr;
#endif

    // Every pinned worker runs the kernel once (tpool_add_work would hand a
    // single job to an arbitrary worker); the result is the per-core rate.
    tpool_run_all(tm, cache_thread_func, (void *)&bm, &start, &end);

#ifdef __linux__
    std::atomic<uint64_t> cycle_count(0);
    std::atomic<int> cycle_samples(0);
    bm.cycle_count = &cycle_count;
    bm.cycle_samples = &cycle_samples;
#endif
    double perf = 0.0;
    const double instructions =
        (double)item.loop_time * ((double)inner_loop * item.inst_pl + 4);
    if (tpool_run_all(tm, cache_thread_func, (void *)&bm, &start, &end)) {
        const double time_used = get_time(&start, &end);
#ifdef __linux__
        const uint64_t cycles = cycle_count.load(std::memory_order_relaxed);
        if (cycles > 0 &&
            cycle_samples.load(std::memory_order_relaxed) ==
                (int)tm->thread_num)
            perf = instructions * tm->thread_num / cycles;
#endif
        if (perf <= 0.0 && time_used > 0.0 && !freq.empty() && freq[0] > 0.0) {
            perf = instructions / (time_used * freq[0] * 1e9);
            warn_estimated_cycles_once();
        }
    }
    stringstream ss;

    if (perf > 0.0)
        ss << setprecision(5) << perf;
    else
        ss << "-";

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
    free(cache_data);
}

//comupte: Instruction Set / Core Computation / Peak Performance / IPC
//cachesize: cache level / Core Computation / bandwith / size / IPC / way
//frequency : core id / theory freq / test freq
//multi issue
static void init_table(vector<Table *> &tables)
{
    tables.resize(5);
    for (int i = 0; i < 5; i++) {
        tables[i] = new Table();
    }

    vector<string> ti;

    ti.resize(5);
    ti[0] = "Instruction Set";
    ti[1] = "Core Computation";
    ti[2] = "Peak Performance";
    ti[3] = "IPC";
    ti[4] = "Latency (cycles)";
    tables[0]->setColumnNum(ti.size());
    tables[0]->addOneItem(ti);

    ti.resize(9);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwidth (per core)";
    ti[3] = "Theory Size";
    ti[4] = "Test Size";
    ti[5] = "Bandwidth (GB/s)";
    ti[6] = "Workset";
    ti[7] = "Load IPC";
    ti[8] = "Bytes/Load";
    tables[1]->setColumnNum(ti.size());
    tables[1]->addOneItem(ti);

    ti.resize(6);
    ti[0] = "Item";
    ti[1] = "Topology / Core";
    ti[2] = "Probe / Kernel";
    ti[3] = "Median Bandwidth";
    ti[4] = "Workset";
    ti[5] = "Measurement";
    tables[2]->setColumnNum(ti.size());
    tables[2]->addOneItem(ti);

    ti.resize(7);
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "Test Freq";
    ti[3] = "IPC(FSU32)";
    ti[4] = "IPC(FSU64)";
    ti[5] = "IPC(LSU ldr)";
    ti[6] = "Counter Source";
    tables[3]->setColumnNum(ti.size());
    tables[3]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Core Instruction";
    ti[2] = "IPC";
    tables[4]->setColumnNum(ti.size());
    tables[4]->addOneItem(ti);
}

static bool prepare_instruction_sweep(const vector<int> &threads, int, void *)
{
    Table freq_table;
    vector<string> freq_head(7);
    freq_head[0] = "Core ID";
    freq_head[1] = "Theory Freq";
    freq_head[2] = "Test Freq";
    freq_head[3] = "IPC(FSU32)";
    freq_head[4] = "IPC(FSU64)";
    freq_head[5] = "IPC(LSU ldr)";
    freq_head[6] = "Counter Source";
    freq_table.setColumnNum(freq_head.size());
    freq_table.addOneItem(freq_head);
    vector<int> mutable_threads = threads;
    get_cpu_freq(mutable_threads, freq_table);
    return true;
}

static bool measure_instruction_sweep(const vector<int> &active_threads,
    uint32_t idle_time, int benchmark_index, int latency_index,
    SweepSample &sample, void *)
{
    tpool_t *tm = tpool_create(active_threads);
    if (tm == nullptr) return false;
    sleep(idle_time);

    if (latency_index >= 0) {
        cpubm_t latency_item = bm_list[latency_index];
        double latency = cpubm_x64_latency(tm, latency_item);
        if (latency > 0.0) sample.latency = format_latency_cycles(latency);
    }
    cpubm_t selected = bm_list[benchmark_index];
    ComputeResult result = cpubm_run_compute(tm, selected);
    sample.performance = result.perf;
    sample.ipc = result.ipc;
    tpool_destroy(tm);
    return true;
}

static bool cpubm_do_instruction_sweep(vector<int> &set_of_threads,
    uint32_t idle_time, const string &instruction,
    const SaveOptions &save_options)
{
    SweepConfig config;
    config.ipc_column = "IPC";
    config.latency_column = "Latency (cycles)";
    return run_instruction_sweep(set_of_threads, idle_time, instruction,
        build_benchmark_catalog(), save_options, config,
        prepare_instruction_sweep, measure_instruction_sweep, nullptr);
}

static bool cpubm_do_bench(vector<int> &set_of_threads, uint32_t idle_time,
    const BenchmarkFilter &filter, const SaveOptions &save_options,
    const CliOptions &options)
{
    if (bm_list.empty()) return false;
    printf("Number Threads: %zu\nThread Pool Binding:", set_of_threads.size());
    for (size_t i = 0; i < set_of_threads.size(); ++i)
        printf(" %d", set_of_threads[i]);
    printf("\n");

    vector<Table *> tables;
    init_table(tables);
    if (benchmark_needs_freq(filter)) get_cpu_freq(set_of_threads, *tables[3]);
    if (should_run_test(filter, "cache") &&
        !cpubm_x64_cache(set_of_threads, *tables[2], options)) {
        return false;
    } else if (should_run_test(filter, "load"))
        get_theory_cache(&cache_size, set_of_threads[0]);

    tpool_t *tm = tpool_create(set_of_threads);
    if (tm == nullptr) {
        cerr << "Error: failed to create benchmark thread pool." << endl;
        for (size_t i = 0; i < tables.size(); ++i) delete tables[i];
        return false;
    }
    BenchmarkCatalog catalog = build_benchmark_catalog();
    uint32_t benches_run = 0;
    for (size_t i = 0; i < bm_list.size(); ++i) {
        if (options.bench_limit > 0 && benches_run >= options.bench_limit)
            break;
        if (catalog[i].is_latency) continue;
        if (!should_run_benchmark(filter, bm_list[i].isa, bm_list[i].dim))
            continue;
        ++benches_run;
        switch (bm_list[i].kind) {
        case BENCHMARK_COMPUTE: {
            double latency = 0.0;
            if (catalog[i].pair_index >= 0) {
                sleep(idle_time);
                latency = cpubm_x64_latency(tm, bm_list[catalog[i].pair_index]);
            }
            sleep(idle_time);
            cpubm_x64_one(tm, bm_list[i], latency, *tables[0]);
            break;
        }
        case BENCHMARK_LOAD:
            sleep(idle_time);
            cpubm_x64_load(tm, bm_list[i], *tables[1]);
            break;
        case BENCHMARK_MULTI_ISSUE:
            sleep(idle_time);
            cpubm_x64_multiple_issue(tm, bm_list[i], *tables[4]);
            break;
        }
    }

    bool ok = print_and_save_benchmark_tables(filter, save_options, tables);
    tpool_destroy(tm);
    for (size_t i = 0; i < tables.size(); ++i) delete tables[i];
    return ok;
}

static void cpufb_register_isa()
{
    const struct cpufb_x86_runtime_features runtime_features =
        cpufb_x86_detect_runtime_features();
    bm_list.clear();
#ifdef _AMX_TILE_
    const bool amx_enabled =
        runtime_features.amx_tile && request_tile_data_permission();
    if (amx_enabled) init_tile_cfg();
#endif

#ifdef _AVX2_
    if (runtime_features.avx2) {
        reg_new_isa("AVX2", "ADD(s32,s32)_latency", "OPS", 0x4000000LL, 128LL,
            nullptr, avx2_add_s32_latency);
        reg_new_isa("AVX2", "ADD(s32,s32)", "OPS", 0x4000000LL, 128LL, nullptr,
            avx2_add_s32);
        reg_new_isa("AVX2", "MUL(s32,s32)_latency", "OPS", 0x4000000LL, 128LL,
            nullptr, avx2_mul_s32_latency);
        reg_new_isa("AVX2", "MUL(s32,s32)", "OPS", 0x4000000LL, 128LL, nullptr,
            avx2_mul_s32);
    }
#endif

#ifdef _AVX512_IFMA_
    if (runtime_features.avx512_ifma) {
        reg_new_isa("AVX512_IFMA", "MADD52(u64,u52,u52)_latency", "OPS",
            0x4000000LL, 256LL, nullptr, avx512_ifma_madd52_u64_latency);
        reg_new_isa("AVX512_IFMA", "MADD52(u64,u52,u52)", "OPS", 0x4000000LL,
            256LL, nullptr, avx512_ifma_madd52_u64);
    }
#endif

#ifdef _AVX512_VBMI_
    if (runtime_features.avx512_vbmi) {
        reg_new_isa("AVX512_VBMI", "PERMB(u8)_latency", "OPS", 0x4000000LL,
            1024LL, nullptr, avx512_vbmi_permb_u8_latency);
        reg_new_isa("AVX512_VBMI", "PERMB(u8)", "OPS", 0x4000000LL, 1024LL,
            nullptr, avx512_vbmi_permb_u8);
    }
#endif

#ifdef _AVX512_VPOPCNTDQ_
    if (runtime_features.avx512_vpopcntdq) {
        reg_new_isa("AVX512_VPOPCNTDQ", "POPCNT(s32)_latency", "OPS",
            0x4000000LL, 256LL, nullptr, avx512_vpopcnt_s32_latency);
        reg_new_isa("AVX512_VPOPCNTDQ", "POPCNT(s32)", "OPS", 0x4000000LL,
            256LL, nullptr, avx512_vpopcnt_s32);
    }
#endif

#ifdef _AMX_INT8_
    if (amx_enabled && runtime_features.amx_int8) {
        reg_new_isa("AMX_INT8", "MM(s32,s8,s8)_latency", "OPS", 0x2500000LL,
            131072LL, &__tilecfg, amx_int8_mm_s32s8s8_latency, 4);
        reg_new_isa("AMX_INT8", "MM(s32,s8,s8)", "OPS", 0x2500000LL, 131072LL,
            &__tilecfg, amx_int8_mm_s32s8s8, 4);
        reg_new_isa("AMX_INT8", "MM(s32,s8,u8)", "OPS", 0x2500000LL, 131072LL,
            &__tilecfg, amx_int8_mm_s32s8u8, 4);
        reg_new_isa("AMX_INT8", "MM(s32,u8,s8)", "OPS", 0x2500000LL, 131072LL,
            &__tilecfg, amx_int8_mm_s32u8s8, 4);
        reg_new_isa("AMX_INT8", "MM(s32,u8,u8)", "OPS", 0x2500000LL, 131072LL,
            &__tilecfg, amx_int8_mm_s32u8u8, 4);
    }
#endif

#ifdef _AMX_BF16_
    if (amx_enabled && runtime_features.amx_bf16) {
        reg_new_isa("AMX_BF16", "MM(f32,bf16,bf16)_latency", "FLOPS",
            0x2500000LL, 65536LL, &__tilecfg, amx_bf16_mm_f32bf16bf16_latency,
            4);
        reg_new_isa("AMX_BF16", "MM(f32,bf16,bf16)", "FLOPS", 0x2500000LL,
            65536LL, &__tilecfg, amx_bf16_mm_f32bf16bf16, 4);
    }
#endif

#ifdef _AVX512_VNNI_
    if (runtime_features.avx512_vnni) {
        reg_new_isa("AVX512_VNNI", "DP4A(s32,u8,s8)_latency", "OPS",
            0x4000000LL, 2048LL, nullptr, avx512_vnni_dp4a_s32u8s8_latency);
        reg_new_isa("AVX512_VNNI", "DP4A(s32,u8,s8)", "OPS", 0x20000000LL,
            2048LL, nullptr, avx512_vnni_dp4a_s32u8s8);
        reg_new_isa("AVX512_VNNI", "DP2A(s32,s16,s16)_latency", "OPS",
            0x4000000LL, 1024LL, nullptr, avx512_vnni_dp2a_s32s16s16_latency);
        reg_new_isa("AVX512_VNNI", "DP2A(s32,s16,s16)", "OPS", 0x20000000LL,
            1024LL, nullptr, avx512_vnni_dp2a_s32s16s16);
    }
#endif

#ifdef _AVX_VNNI_
    if (runtime_features.avx_vnni) {
        reg_new_isa("AVX_VNNI", "DP4A(s32,u8,s8)_latency", "OPS", 0x4000000LL,
            1024LL, nullptr, avx_vnni_dp4a_s32u8s8_latency);
        reg_new_isa("AVX_VNNI", "DP4A(s32,u8,s8)", "OPS", 0x20000000LL, 1024LL,
            nullptr, avx_vnni_dp4a_s32u8s8);
        reg_new_isa("AVX_VNNI", "DP2A(s32,s16,s16)_latency", "OPS", 0x4000000LL,
            512LL, nullptr, avx_vnni_dp2a_s32s16s16_latency);
        reg_new_isa("AVX_VNNI", "DP2A(s32,s16,s16)", "OPS", 0x20000000LL, 512LL,
            nullptr, avx_vnni_dp2a_s32s16s16);
    }
#endif

#ifdef _AVX_VNNI_INT8_
    if (runtime_features.avx_vnni_int8) {
        reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,s8,s8)", "OPS", 0x20000000LL,
            1024LL, nullptr, avx_vnni_int8_dp4a_s32s8s8);
        reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,s8,u8)", "OPS", 0x20000000LL,
            1024LL, nullptr, avx_vnni_int8_dp4a_s32s8u8);
        reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,u8,u8)", "OPS", 0x20000000LL,
            1024LL, nullptr, avx_vnni_int8_dp4a_s32u8u8);
    }
#endif

#ifdef _AVX512_BF16_
    if (runtime_features.avx512_bf16) {
        reg_new_isa("AVX512_BF16", "DP2A(f32,bf16,bf16)_latency", "FLOPS",
            0x4000000LL, 1024LL, nullptr, avx512_bf16_dp2a_f32bf16bf16_latency);
        reg_new_isa("AVX512_BF16", "DP2A(f32,bf16,bf16)", "FLOPS", 0x20000000LL,
            1024LL, nullptr, avx512_bf16_dp2a_f32bf16bf16);
    }
#endif

#ifdef _AVX512_FP16_
    if (runtime_features.avx512_fp16) {
        reg_new_isa("AVX512_FP16", "FMA(f16,f16,f16)_latency", "FLOPS",
            0x4000000LL, 1024LL, nullptr, avx512_fp16_fma_f16f16f16_latency);
        reg_new_isa("AVX512_FP16", "FMA(f16,f16,f16)", "FLOPS", 0x20000000LL,
            1024LL, nullptr, avx512_fp16_fma_f16f16f16);
    }
#endif

#ifdef _AVX512F_
    if (runtime_features.avx512f) {
        reg_new_isa("AVX512F", "FMA(f32,f32,f32)_latency", "FLOPS", 0x4000000LL,
            512LL, nullptr, avx512f_fma_f32f32f32_latency);
        reg_new_isa("AVX512F", "FMA(f32,f32,f32)", "FLOPS", 0x20000000LL, 512LL,
            nullptr, avx512f_fma_f32f32f32);
        reg_new_isa("AVX512F", "FMA(f64,f64,f64)_latency", "FLOPS", 0x4000000LL,
            256LL, nullptr, avx512f_fma_f64f64f64_latency);
        reg_new_isa("AVX512F", "FMA(f64,f64,f64)", "FLOPS", 0x20000000LL, 256LL,
            nullptr, avx512f_fma_f64f64f64);
    }
#endif

#ifdef _FMA_
    if (runtime_features.fma) {
        reg_new_isa("FMA", "FMA(f32,f32,f32)_latency", "FLOPS", 0x4000000LL,
            256LL, nullptr, fma_f32f32f32_latency);
        reg_new_isa("FMA", "FMA(f32,f32,f32)", "FLOPS", 0x20000000LL, 256LL,
            nullptr, fma_f32f32f32);
        reg_new_isa("FMA", "FMA(f64,f64,f64)_latency", "FLOPS", 0x4000000LL,
            128LL, nullptr, fma_f64f64f64_latency);
        reg_new_isa("FMA", "FMA(f64,f64,f64)", "FLOPS", 0x20000000LL, 128LL,
            nullptr, fma_f64f64f64);
    }
#endif

#ifdef _AVX_
    if (runtime_features.avx) {
        reg_new_isa("AVX", "ADD(f32,f32)_latency", "FLOPS", 0x4000000LL, 128LL,
            nullptr, avx_add_f32_latency);
        reg_new_isa("AVX", "ADD(f32,f32)", "FLOPS", 0x4000000LL, 128LL, nullptr,
            avx_add_f32);
        reg_new_isa("AVX", "MUL(f32,f32)_latency", "FLOPS", 0x4000000LL, 128LL,
            nullptr, avx_mul_f32_latency);
        reg_new_isa("AVX", "MUL(f32,f32)", "FLOPS", 0x4000000LL, 128LL, nullptr,
            avx_mul_f32);
        reg_new_isa("AVX", "ADD(f64,f64)_latency", "FLOPS", 0x4000000LL, 64LL,
            nullptr, avx_add_f64_latency);
        reg_new_isa("AVX", "ADD(f64,f64)", "FLOPS", 0x4000000LL, 64LL, nullptr,
            avx_add_f64);
        reg_new_isa("AVX", "MUL(f64,f64)_latency", "FLOPS", 0x4000000LL, 64LL,
            nullptr, avx_mul_f64_latency);
        reg_new_isa("AVX", "MUL(f64,f64)", "FLOPS", 0x4000000LL, 64LL, nullptr,
            avx_mul_f64);
        reg_new_isa("AVX", "ADD(MUL(f32,f32),f32)_latency", "FLOPS",
            0x4000000LL, 128LL, nullptr, avx_add_mul_f32f32_f32_latency);
        reg_new_isa("AVX", "ADD(MUL(f32,f32),f32)", "FLOPS", 0x20000000LL,
            128LL, nullptr, avx_add_mul_f32f32_f32);
        reg_new_isa("AVX", "ADD(MUL(f64,f64),f64)_latency", "FLOPS",
            0x4000000LL, 64LL, nullptr, avx_add_mul_f64f64_f64_latency);
        reg_new_isa("AVX", "ADD(MUL(f64,f64),f64)", "FLOPS", 0x20000000LL, 64LL,
            nullptr, avx_add_mul_f64f64_f64);
    }
#endif

#ifdef _SSE_
    if (runtime_features.sse) {
        reg_new_isa("SSE", "ADD(f32,f32)_latency", "FLOPS", 0x4000000LL, 64LL,
            nullptr, sse_add_f32_latency);
        reg_new_isa("SSE", "ADD(f32,f32)", "FLOPS", 0x4000000LL, 64LL, nullptr,
            sse_add_f32);
        reg_new_isa("SSE", "MUL(f32,f32)_latency", "FLOPS", 0x4000000LL, 64LL,
            nullptr, sse_mul_f32_latency);
        reg_new_isa("SSE", "MUL(f32,f32)", "FLOPS", 0x4000000LL, 64LL, nullptr,
            sse_mul_f32);
        reg_new_isa("SSE", "ADD(MUL(f32,f32),f32)", "FLOPS", 0x20000000LL, 64LL,
            nullptr, sse_add_mul_f32f32_f32);
    }
#endif

#ifdef _SSE2_
    if (runtime_features.sse2) {
        reg_new_isa("SSE2", "ADD(f64,f64)_latency", "FLOPS", 0x4000000LL, 32LL,
            nullptr, sse2_add_f64_latency);
        reg_new_isa("SSE2", "ADD(f64,f64)", "FLOPS", 0x4000000LL, 32LL, nullptr,
            sse2_add_f64);
        reg_new_isa("SSE2", "MUL(f64,f64)_latency", "FLOPS", 0x4000000LL, 32LL,
            nullptr, sse2_mul_f64_latency);
        reg_new_isa("SSE2", "MUL(f64,f64)", "FLOPS", 0x4000000LL, 32LL, nullptr,
            sse2_mul_f64);
        reg_new_isa("SSE2", "ADD(MUL(f64,f64),f64)", "FLOPS", 0x20000000LL,
            32LL, nullptr, sse2_add_mul_f64f64_f64);
    }
#endif

    for (int level = 1; level <= 2; ++level) {
        // Only the first row of a level carries its label.
        string label = level == 1 ? "L1 Cache" : "L2 Cache";
        if (runtime_features.avx) {
            reg_load_kernel(
                label, "vmovups.ymm(f32)", level, load_vmovups_kernel, 32);
            label = "--------";
        }
        reg_load_kernel(
            label, "movss.scalar(f32)", level, load_movss_stream_kernel, 4);
        reg_load_kernel(
            "--------", "movups.xmm(f32)", level, load_movups_xmm_kernel, 16);
#ifdef _AVX512F_
        if (runtime_features.avx512f)
            reg_load_kernel("--------", "vmovups.zmm(f32)", level,
                load_vmovups_zmm_kernel, 64);
#endif
    }

    // 32 benchmarked instructions plus the two loop-control instructions.
    if (runtime_features.sse)
        reg_multi_issue("MULTI_ISSUE", "movups/mulps.xmm", 34, multiple_issue);
#ifdef _FMA_
    if (runtime_features.fma)
        reg_multi_issue(
            "MULTI_ISSUE_AVX", "vmovups/vfmadd.ymm", 34, multiple_issue_avx);
#endif
#ifdef _AVX512F_
    if (runtime_features.avx512f && runtime_features.fma)
        reg_multi_issue("MULTI_ISSUE_AVX512", "vmovups/vfmadd.zmm", 34,
            multiple_issue_avx512);
#endif
}

// Same contract as ARM64: divide every registered loop count for smoke runs.
static void scale_benchmark_loops(uint32_t loop_scale)
{
    if (loop_scale <= 1) return;
    for (cpubm_t &bm : bm_list)
        bm.loop_time = max<int64_t>(1, bm.loop_time / loop_scale);
}

int main(int argc, char *argv[])
{
    CliOptions options;
    if (!parse_cli_options(argc, argv, options)) return 1;

    if (options.list_categories || options.list_instructions) {
        cpufb_register_isa();
        BenchmarkCatalog catalog = build_benchmark_catalog();
        if (options.list_categories) print_benchmark_categories(catalog);
        if (options.list_instructions) print_benchmark_instructions(catalog);
        return 0;
    }
    if (!options.thread_pool_set) {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --idle_time parameter.\n");
        fprintf(stderr,
            "Usage: %s --thread_pool=[xxx] [--idle_time=yyy] [--mode=all|cache|compute] [--loop_scale=zzz] [--bench_limit=nnn] [--include-test=list] [--exclude-test=list] [--include-isa=list] [--exclude-isa=list]\n",
            argv[0]);
        fprintf(stderr,
            "       %s --thread_pool=[xxx] --sweep-instruction='Core Computation'\n",
            argv[0]);
        fprintf(stderr,
            "       %s --thread_pool=[cores] --memory-bandwidth [--memory-size-mib=N; default=auto per stream] [--memory-repetitions=5]\n",
            argv[0]);
        fprintf(stderr, "       %s --list-categories | --list-instructions\n",
            argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr,
            "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr,
            "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        return 1;
    }

    if (options.thread_pool.empty()) {
        fprintf(
            stderr, "Error: --thread_pool must contain at least one CPU.\n");
        return 1;
    }
    if (!finalize_save_options(options.save)) return 1;
    if (!validate_memory_bandwidth_options(options, true)) return 1;
    initialize_system_information(options.thread_pool);
    print_system_information();
    if (options.memory_bandwidth) return run_memory_bandwidth(options) ? 0 : 1;

    cpufb_register_isa();
    scale_benchmark_loops(options.loop_scale);
    BenchmarkCatalog catalog = build_benchmark_catalog();
    if (!validate_benchmark_filter(options.filter, catalog)) return 1;
    if (!options.sweep_instruction.empty())
        return cpubm_do_instruction_sweep(options.thread_pool,
                   options.idle_time, options.sweep_instruction, options.save)
            ? 0
            : 1;
    return cpubm_do_bench(options.thread_pool, options.idle_time,
               options.filter, options.save, options)
        ? 0
        : 1;
}
