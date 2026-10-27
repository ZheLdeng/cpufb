#include "cli.hpp"
#include "cache/cache_topology.hpp"
#include "cache/cache_curve.hpp"
#include "cache/prefetch_probe.hpp"
#include "cache/cacheline_probe.hpp"
#include "table.hpp"
#include "sys/thread_pool.hpp"
#include "sys/common.hpp"

#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "frequency.hpp"
#include "compute.hpp"
#include "load.hpp"
#include "benchmark_tables.hpp"
#include <cmath>
#include <algorithm>
#include <atomic>
#include <fstream>
using namespace std;
using namespace cpufb::cli;

struct cpubm_t
{
    std::string isa;
    std::string type;
    std::string dim;
    int64_t loop_time;
    int64_t comp_pl;
    int64_t inst_pl;
    void (*bench)(int64_t);
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

static void reg_new_isa(std::string isa, std::string type, std::string dim,
    int64_t loop_time, int64_t comp_pl, void (*bench)(int64_t),
    int64_t inst_pl = 24)
{
    cpubm_t new_one;
    new_one.isa = isa;
    new_one.type = type;
    new_one.dim = dim;
    new_one.loop_time = loop_time;
    new_one.comp_pl = comp_pl;
    new_one.inst_pl = inst_pl;
    new_one.bench = bench;
#ifdef __linux__
    new_one.cycle_count = nullptr;
    new_one.cycle_samples = nullptr;
#endif

    bm_list.push_back(new_one);
}

static void thread_func(void *params)
{
    cpubm_t *bm = (cpubm_t *)params;
#ifdef __linux__
    if (bm->cycle_count != nullptr && bm->cycle_samples != nullptr) {
        PerfEventCycle cycle_counter(0, false);
        if (cycle_counter.available()) {
            cycle_counter.start();
            bm->bench(bm->loop_time);
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
    bm->bench(bm->loop_time);
}

struct ComputeResult
{
    double perf;
    double ipc;
    bool hardware_cycles;
};

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    struct timespec start, end;
    int num_threads = tm->thread_num;

    cpubm_t warmup = item;
    warmup.loop_time = max<int64_t>(1, item.loop_time / 32);
    tpool_run_all(tm, thread_func, (void *)&warmup, &start, &end);

    ComputeResult result = {0.0, 0.0, false};
    double best_time = 1e30;
    uint64_t best_cycles = 0;
    for (int sample = 0; sample < 5; ++sample) {
        cpubm_t measured = item;
#ifdef __linux__
        std::atomic<uint64_t> cycle_count(0);
        std::atomic<int> cycle_samples(0);
        measured.cycle_count = &cycle_count;
        measured.cycle_samples = &cycle_samples;
#endif
        if (!tpool_run_all(tm, thread_func, (void *)&measured, &start, &end))
            return result;
        const double time_used = get_time(&start, &end);
        if (time_used <= 0.0 || time_used >= best_time) continue;
        best_time = time_used;
#ifdef __linux__
        best_cycles = cycle_samples.load(std::memory_order_relaxed) == num_threads
            ? cycle_count.load(std::memory_order_relaxed)
            : 0;
#endif
    }
    if (best_time == 1e30) return result;

    const double instructions =
        static_cast<double>(item.loop_time) * item.inst_pl * num_threads;
    result.perf = item.loop_time * item.comp_pl * num_threads / best_time;
    if (best_cycles > 0) {
        result.ipc = instructions / best_cycles;
        result.hardware_cycles = true;
    } else {
        double aggregate_frequency_hz = 0.0;
        for (int worker = 0;
            worker < num_threads && worker < static_cast<int>(freq.size());
            ++worker) {
            if (freq[worker] > 0.0)
                aggregate_frequency_hz += freq[worker] * 1e9;
        }
        if (aggregate_frequency_hz <= 0.0) return result;
        result.ipc = instructions / (best_time * aggregate_frequency_hz);
        cpufb::warn_estimated_cycles_once("the frequency table's");
    }
    return result;
}

static double cpubm_riscv64_latency(tpool_t *tm, cpubm_t &item)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    return result.ipc > 0.0 ? 1.0 / result.ipc : 0.0;
}

static void cpubm_riscv64_one(
    tpool_t *tm, cpubm_t &item, double latency, Table &table)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    vector<string> cont(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_perf_value(result.perf, item.dim);
    cont[3] = result.ipc > 0.0 ? to_string(result.ipc) : "-";
    cont[4] = latency > 0.0 ? cpufb::format_fixed(latency, 2, "") : "-";
    table.addOneItem(cont);
}

// Reported values, the latency curve, the line and set-conflict probes and
// the rows are shared (common/cache/cache_report.hpp); this file supplies the
// kernels.  riscv64 has no memory stream kernel, so no DRAM rows follow.
static CacheData cache_size;

static void cpubm_riscv64_cache(std::vector<int> &set_of_threads, Table &table)
{
    const int cpu = set_of_threads[0];
    cpufb::read_reported_cache(cache_size, cpu);
    cpufb::measure_cache_report(cache_size, cpu, cache_probe_kernels());
    cpufb::append_cache_report_rows(
        table, cache_size, cpu, freq.empty() ? 0.0 : freq[0]);
}

#ifdef _VECTOR_
// L1/L2 rows and the fixed 1:1 load+FMA stream through the shared harness
// (cache_bandwidth.hpp).  The RVV kernels take (data, bytes, passes), so a
// count unit of one byte makes them LoadKernels.
static cpufb::LoadKernel rvv_stream_kernel(
    const char *name, void (*function)(void *, size_t, int64_t))
{
    cpufb::LoadKernel kernel;
    kernel.name = name;
    kernel.function = reinterpret_cast<void (*)(float *, int, int64_t)>(function);
    kernel.bytes_per_count = 1;
    kernel.block_bytes = riscv_vector_length_bytes();
    kernel.bytes_per_load = kernel.block_bytes;
    return kernel;
}

static void add_riscv64_load_rows(tpool_t *tm, Table &table)
{
    if (riscv_vector_length_bytes() == 0) return;
    const cpufb::LoadKernel kernel =
        rvv_stream_kernel("rvv-vle8.v", vector_load_stream);
    for (int level = 1; level <= 2; ++level)
        cpufb::append_load_bandwidth_row(table,
            level == 1 ? "L1 Cache" : "L2 Cache", kernel, level, cache_size, tm,
            freq);
}

// A fixed 1:1 stream of vle32.v and vfmacc.vv over half of L1: two
// instructions per vector loaded, so the IPC is twice the load rate.
static void add_riscv64_multiple_issue(tpool_t *tm, Table &table)
{
    const size_t l1_kib = static_cast<size_t>(
        cache_size.theory_L1 > 0 ? cache_size.theory_L1 : cache_size.test_L1);
    if (riscv_vector_length_bytes() == 0 || l1_kib == 0) return;
    const cpufb::LoadKernel kernel =
        rvv_stream_kernel("vle32.v + vfmacc.vv", vector_load_fma_stream);
    double clock_hz = 0.0;
    for (const double ghz : freq) clock_hz = std::max(clock_hz, ghz * 1e9);
    const cpufb::CacheBandwidth bandwidth = cpufb::measure_cache_bandwidth(
        kernel, cpufb::cache_level_workset(0, l1_kib * 1024), tm, clock_hz);
    vector<string> row(table.getCol());
    row[0] = "RVV compute:load";
    row[1] = kernel.name;
    row[2] = cpufb::format_or_dash(2.0 * bandwidth.load_ipc, 3, "");
    row[3] = "fixed 1:1 stream";
    row[4] = bandwidth.cycle_source.empty()
        ? "not available"
        : "no ratio search on riscv64; " + bandwidth.cycle_source;
    table.addOneItem(row);
}
#endif

static bool cpubm_do_bench(std::vector<int> &set_of_threads, uint32_t idle_time,
    const BenchmarkFilter &filter, const SaveOptions &save_options)
{
    int i;

    if (bm_list.size() > 0) {
        int num_threads = set_of_threads.size();

        printf("Number Threads: %d\n", num_threads);
        printf("Thread Pool Binding:");
        for (i = 0; i < num_threads; i++) {
            printf(" %d", set_of_threads[i]);
        }
        printf("\n");

        // set table head
        vector<Table *> tables;
        cpufb::init_benchmark_tables(tables, false);

        // set thread pool
        tpool_t *tm;
        tm = tpool_create(set_of_threads);
        if (tm == nullptr) {
            cerr << "Error: failed to create benchmark thread pool." << endl;
            for (Table *table : tables) delete table;
            return false;
        }
        BenchmarkCatalog catalog = build_benchmark_catalog();

        if (benchmark_needs_freq(filter))
            get_cpu_freq(set_of_threads, *tables[3]);

        if (should_run_test(filter, "cache"))
            cpubm_riscv64_cache(set_of_threads, *tables[2]);
#ifdef _VECTOR_
        if (should_run_test(filter, "load"))
            cpufb::prepare_load_capacities(cache_size, set_of_threads[0]);
            add_riscv64_load_rows(tm, *tables[1]);
        if (should_run_test(filter, "multi_issue"))
            cpufb::prepare_load_capacities(cache_size, set_of_threads[0]);
            add_riscv64_multiple_issue(tm, *tables[4]);
#endif

        // traverse task list

        for (i = 0; i < static_cast<int>(bm_list.size()); i++) {
            if (catalog[i].is_latency) continue;
            if (!should_run_benchmark(filter, bm_list[i].isa, bm_list[i].dim))
                continue;
            if (bm_list[i].dim.find("OPS") != string::npos) {
                double latency = 0.0;
                if (catalog[i].pair_index >= 0) {
                    sleep(idle_time);
                    latency = cpubm_riscv64_latency(
                        tm, bm_list[catalog[i].pair_index]);
                }
                sleep(idle_time);
                cpubm_riscv64_one(tm, bm_list[i], latency, *tables[0]);
            } else {
                cout << "Wrong dimension !" << endl;
                break;
            }
        }

        record_migration_information(tpool_get_migration_info(tm));
        const bool save_ok =
            print_and_save_benchmark_tables(filter, save_options, tables);

        tpool_destroy(tm);
        for (Table *table : tables) delete table;
        return save_ok;
    }
    printf("Sorry, there's no any supported SIMD isa.\n");
    return false;
}

static void cpufb_register_isa()
{
#ifdef _IME_
    reg_new_isa("ime", "vmadot(s32,s8,s8)", "OPS", 0x10000000LL, 3584LL,
        ime_vmadot_s32s8s8);
    reg_new_isa("ime", "vmadotu(u32,u8,u8)", "OPS", 0x10000000LL, 3584LL,
        ime_vmadotu_u32u8u8);
    reg_new_isa("ime", "vmadotus(s32,u8,s8)", "OPS", 0x10000000LL, 3584LL,
        ime_vmadotus_s32u8s8);
    reg_new_isa("ime", "vmadotsu(s32,s8,u8)", "OPS", 0x10000000LL, 3584LL,
        ime_vmadotsu_s32s8u8);
    reg_new_isa("ime", "vmadotslide(s32,s8,s8)", "OPS", 0x10000000LL, 3072LL,
        ime_vmadotslide_s32s8s8);
#endif

#ifdef _VECTOR_
    size_t avl = 0;
    __asm__ volatile("vsetvli %[avl], x0, e16, m1\n\t"
        : [avl] "=r"(avl)
        :
        : "cc");
    reg_new_isa("vector", "vfmacc.vf(f16,f16,f16)", "FLOPS", 0x10000000LL,
        48LL * avl, vector_vfmacc_vf_f16f16f16);
    reg_new_isa("vector", "vfmacc.vv(f16,f16,f16)", "FLOPS", 0x10000000LL,
        48LL * avl, vector_vfmacc_vv_f16f16f16);

    __asm__ volatile("vsetvli %[avl], x0, e32, m1\n\t"
        : [avl] "=r"(avl)
        :
        : "cc");
    reg_new_isa("vector", "vfmacc.vf(f32,f32,f32)", "FLOPS", 0x10000000LL,
        48LL * avl, vector_vfmacc_vf_f32f32f32);
    reg_new_isa("vector", "vfmacc.vv(f32,f32,f32)_latency", "FLOPS",
        0x100000LL, 48LL * avl, vector_vfmacc_vv_f32f32f32_latency);
    reg_new_isa("vector", "vfmacc.vv(f32,f32,f32)", "FLOPS", 0x10000000LL,
        48LL * avl, vector_vfmacc_vv_f32f32f32);

    __asm__ volatile("vsetvli %[avl], x0, e64, m1\n\t"
        : [avl] "=r"(avl)
        :
        : "cc");
    reg_new_isa("vector", "vfmacc.vf(f64,f64,f64)", "FLOPS", 0x10000000LL,
        48LL * avl, vector_vfmacc_vf_f64f64f64);
    reg_new_isa("vector", "vfmacc.vv(f64,f64,f64)", "FLOPS", 0x10000000LL,
        48LL * avl, vector_vfmacc_vv_f64f64f64);
#endif
}

int main(int argc, char *argv[])
{
    CliOptions options;
    if (!parse_cli_options(argc, argv, options)) return 1;

    // Listing the catalogue needs no cores, and the other two backends
    // answer it before they insist on a thread pool.  Asking for one here
    // made `--list-instructions` exit 1 on this backend alone, which is what
    // the CLI tests found the first time they were allowed to run on it.
    if (options.list_categories || options.list_instructions) {
        cpufb_register_isa();
        BenchmarkCatalog listing = build_benchmark_catalog();
        if (options.list_categories) print_benchmark_categories(listing);
        if (options.list_instructions) print_benchmark_instructions(listing);
        return 0;
    }

    if (!options.thread_pool_set || options.thread_pool.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    if (options.memory_bandwidth || options.memory_size_set ||
        options.memory_repetitions_set || !options.sweep_instruction.empty() ||
        options.loop_scale != 1 || options.bench_limit != 0) {
        fprintf(stderr,
            "Error: the riscv64 backend does not support memory-bandwidth, "
            "instruction sweep, loop scaling, or benchmark limits.\n");
        return 1;
    }

    if (!finalize_save_options(options.save)) return 1;
    initialize_system_information(options.thread_pool);
    print_system_information();
    cpufb_register_isa();
    BenchmarkCatalog catalog = build_benchmark_catalog();
    if (!validate_benchmark_filter(options.filter, catalog)) return 1;
    return cpubm_do_bench(options.thread_pool, options.idle_time,
               options.filter, options.save)
        ? 0
        : 1;
}
