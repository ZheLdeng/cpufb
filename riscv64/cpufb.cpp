#include "cli.hpp"
#include "cache_topology.hpp"
#include "cache_curve.hpp"
#include "cacheline_probe.hpp"
#include "table.hpp"
#include "thread_pool.hpp"

#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <frequency.hpp>
#include <compute.hpp>
#include <load.hpp>
#include <cmath>
#include <algorithm>
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
    void (*bench)(int64_t);
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

static double get_time(struct timespec *start, struct timespec *end)
{
    return end->tv_sec - start->tv_sec + (end->tv_nsec - start->tv_nsec) * 1e-9;
}

static void reg_new_isa(std::string isa, std::string type, std::string dim,
    int64_t loop_time, int64_t comp_pl, void (*bench)(int64_t))
{
    cpubm_t new_one;
    new_one.isa = isa;
    new_one.type = type;
    new_one.dim = dim;
    new_one.loop_time = loop_time;
    new_one.comp_pl = comp_pl;
    new_one.bench = bench;

    bm_list.push_back(new_one);
}

static void thread_func(void *params)
{
    cpubm_t *bm = (cpubm_t *)params;
    bm->bench(bm->loop_time);
}

struct ComputeResult
{
    double perf;
    double ipc;
};

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    struct timespec start, end;
    int i;
    int num_threads = tm->thread_num;

    // warm up
    for (i = 0; i < num_threads; i++) {
        tpool_add_work(tm, thread_func, (void *)&item);
    }
    tpool_wait(tm);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (i = 0; i < num_threads; i++) {
        tpool_add_work(tm, thread_func, (void *)&item);
    }
    tpool_wait(tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);

    double time_used = get_time(&start, &end);
    ComputeResult result;
    result.perf = item.loop_time * item.comp_pl * num_threads / time_used;
    result.ipc =
        item.loop_time * 24 * tm->thread_num / time_used / freq[0] / 1e9;
    return result;
}

static int64_t cpubm_riscv64_latency(tpool_t *tm, cpubm_t &item)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    return result.ipc > 0.0 ? static_cast<int64_t>(round(1.0 / result.ipc)) : 0;
}

static void cpubm_riscv64_one(
    tpool_t *tm, cpubm_t &item, int64_t latency, Table &table)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    vector<string> cont(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_perf_value(result.perf, item.dim);
    cont[3] = to_string(result.ipc);
    cont[4] = latency > 0 ? to_string(latency) : "-";
    table.addOneItem(cont);
}

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
    ti[4] = "Latency";
    tables[0]->setColumnNum(ti.size());
    tables[0]->addOneItem(ti);

    ti.resize(8);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwidth (per core)";
    ti[3] = "Cache Capacity";
    ti[4] = "Workset";
    ti[5] = "Capacity Source";
    ti[6] = "Bandwidth (GB/s)";
    ti[7] = "Cycle Source";
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

#ifdef _SVE_
    ti.resize(8);
#else
    ti.resize(7);
#endif
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "Test Freq";
    ti[3] = "IPC(FSU32)";
    ti[4] = "IPC(FSU64)";
    ti[5] = "IPC(LSU ldr)";
    ti[6] = "Counter Source";
#ifdef _SVE_
    ti[6] = "IPC(SVE32)";
    ti[7] = "IPC(SVE64)";
#endif
    tables[3]->setColumnNum(ti.size());
    tables[3]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Core Instruction";
    ti[2] = "IPC";
    tables[4]->setColumnNum(ti.size());
    tables[4]->addOneItem(ti);
}

static int read_cache_integer(int cpu, int index, const char *name)
{
    const string path = "/sys/devices/system/cpu/cpu" + to_string(cpu) +
        "/cache/index" + to_string(index) + "/" + name;
    ifstream input(path.c_str());
    int value = 0;
    return input >> value ? value : 0;
}

static void cpubm_riscv64_cache(std::vector<int> &set_of_threads, Table &table)
{
    const int cpu = set_of_threads[0];
    const cpufb::CacheLevelInfo l1 = cpufb::detect_data_cache_level(cpu, 1);
    const cpufb::CacheLevelInfo l2 = cpufb::detect_data_cache_level(cpu, 2);
    int cache_index = 0;
    while (
        cache_index < 32 && read_cache_integer(cpu, cache_index, "level") != 1)
        ++cache_index;
    const int reported_line =
        read_cache_integer(cpu, cache_index, "coherency_line_size");
    const int reported_ways =
        read_cache_integer(cpu, cache_index, "ways_of_associativity");
    const cpufb::CacheCurveResult curve =
        cpufb::measure_cache_curve(riscv_cache_chase32,
            cpufb::effective_cacheline_size(reported_line, 0, 64),
            64ULL * 1024 * 1024);
    const auto measured_capacity = [&curve](const string &level) {
        for (const cpufb::CacheLevelEstimate &estimate : curve.levels)
            if (estimate.level == level) return estimate.capacity_bytes;
        return uint64_t(0);
    };
    cpufb::debug_print_cache_curve(curve);
    const uint64_t measured_l1 = measured_capacity("L1");
    const uint64_t measured_l2 = measured_capacity("L2");
    vector<string> cont(table.getCol());
    cont[0] = "L1 data cache capacity";
    cont[1] = l1.bytes > 0 ? cpufb::format_cache_capacity(l1.bytes) : "-";
    cont[2] = measured_l1 > 0 ? cpufb::format_cache_capacity(measured_l1) : "-";
    const string doubt = cpufb::describe_prefetch_doubt(curve);
    cont[5] = cpufb::describe_probe_agreement(l1.bytes, measured_l1, 1.5);
    if (!doubt.empty()) cont[5] += "; " + doubt;
    table.addOneItem(cont);

    cont.assign(table.getCol(), "");
    cont[0] = "L2/unified cache capacity";
    cont[1] = l2.bytes > 0 ? cpufb::format_cache_capacity(l2.bytes) : "-";
    cont[2] = measured_l2 > 0 ? cpufb::format_cache_capacity(measured_l2) : "-";
    cont[5] = cpufb::describe_probe_agreement(l2.bytes, measured_l2, 1.5);
    if (!doubt.empty()) cont[5] += "; " + doubt;
    table.addOneItem(cont);

    // Both probes run after the curve: the line probe sizes its eviction
    // buffer from the largest level the curve measured, and the
    // associativity ring steps by the line size the line probe found.  These
    // two rows used to be verbatim copies of the OS column.
    const CacheGeometryProbe geometry = probe_cache_geometry(
        reported_line, std::max(measured_l1, measured_l2) * 4);
    cont.assign(table.getCol(), "");
    cont[0] = "L1 ways of associativity";
    cont[1] = reported_ways > 0 ? to_string(reported_ways) : "-";
    cont[2] = geometry.l1_ways > 0 ? to_string(geometry.l1_ways) : "-";
    cont[5] =
        cpufb::describe_probe_agreement(reported_ways, geometry.l1_ways, 1.0);
    table.addOneItem(cont);

    cont.assign(table.getCol(), "");
    cont[0] = "cacheline size";
    cont[1] = reported_line > 0 ? to_string(reported_line) + " B" : "-";
    cont[2] = geometry.cacheline_bytes > 0
        ? to_string(geometry.cacheline_bytes) + " B"
        : "-";
    cont[5] = cpufb::describe_probe_agreement(
        reported_line, geometry.cacheline_bytes, 1.0);
    table.addOneItem(cont);

    cout << "Cache curve translation mode: " << curve.translation_mode << endl;
    Table curve_table;
    curve_table.setColumnNum(2);
    vector<string> curve_head = {"Working Set", "Dependent-load Latency"};
    curve_table.addOneItem(curve_head);
    for (const cpufb::CacheLatencyPoint &point : curve.points) {
        vector<string> row(2);
        row[0] = cpufb::format_cache_capacity(point.working_set_bytes);
        ostringstream latency;
        latency << fixed << setprecision(3) << point.latency_ns << " ns/load";
        row[1] = latency.str();
        curve_table.addOneItem(row);
    }
    curve_table.print();

    Table estimate_table;
    estimate_table.setColumnNum(4);
    vector<string> estimate_head = {
        "Level", "Measured Capacity", "Latency", "Jump"};
    estimate_table.addOneItem(estimate_head);
    for (const cpufb::CacheLevelEstimate &estimate : curve.levels) {
        vector<string> row(4);
        row[0] = estimate.level;
        row[1] = cpufb::format_cache_capacity(estimate.capacity_bytes);
        ostringstream latency, jump;
        latency << fixed << setprecision(3) << estimate.latency_ns
                << " ns/load";
        jump << fixed << setprecision(2) << estimate.jump_ratio << "x";
        row[2] = latency.str();
        row[3] = jump.str();
        estimate_table.addOneItem(row);
    }
    estimate_table.print();
}

#ifdef _VECTOR_
struct StreamBenchmark
{
    vector<void *> buffers;
    size_t bytes = 0;
    int64_t repetitions = 0;
    bool mixed = false;
};

static void stream_thread_func(void *params)
{
    StreamBenchmark *benchmark = static_cast<StreamBenchmark *>(params);
    const size_t index = tpool_worker_index();
    if (index >= benchmark->buffers.size()) return;
    if (benchmark->mixed)
        vector_load_fma_stream(benchmark->buffers[index], benchmark->bytes,
            benchmark->repetitions);
    else
        vector_load_stream(benchmark->buffers[index], benchmark->bytes,
            benchmark->repetitions);
}

static double median_stream_seconds(tpool_t *tm, StreamBenchmark &benchmark)
{
    vector<double> samples;
    struct timespec start, end;
    tpool_run_all(tm, stream_thread_func, &benchmark, &start, &end);
    for (int sample = 0; sample < 5; ++sample) {
        if (tpool_run_all(tm, stream_thread_func, &benchmark, &start, &end))
            samples.push_back(get_time(&start, &end));
    }
    if (samples.empty()) return 0.0;
    sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

static bool allocate_stream_buffers(
    StreamBenchmark &benchmark, size_t thread_count)
{
    benchmark.buffers.resize(thread_count, nullptr);
    for (void *&buffer : benchmark.buffers) {
        if (posix_memalign(&buffer, 64, benchmark.bytes) != 0) return false;
        memset(buffer, 1, benchmark.bytes);
    }
    return true;
}

static void free_stream_buffers(StreamBenchmark &benchmark)
{
    for (void *buffer : benchmark.buffers) free(buffer);
}

static void add_riscv64_load_rows(tpool_t *tm, int cpu, Table &table)
{
    const double frequency_hz = !freq.empty() ? freq[0] * 1e9 : 0.0;
    const size_t vector_bytes = riscv_vector_length_bytes();
    for (int level = 1; level <= 2; ++level) {
        const cpufb::CacheLevelInfo cache =
            cpufb::detect_data_cache_level(cpu, level);
        if (cache.bytes == 0 || vector_bytes == 0) continue;
        StreamBenchmark benchmark;
        benchmark.bytes = max<size_t>(vector_bytes, cache.bytes / 2);
        benchmark.bytes -= benchmark.bytes % vector_bytes;
        const uint64_t target_bytes = 256ULL * 1024 * 1024;
        benchmark.repetitions = max<int64_t>(
            1, static_cast<int64_t>(target_bytes / benchmark.bytes));
        if (!allocate_stream_buffers(benchmark, tm->thread_num)) {
            free_stream_buffers(benchmark);
            continue;
        }
        const double seconds = median_stream_seconds(tm, benchmark);
        const double bytes =
            static_cast<double>(benchmark.bytes) * benchmark.repetitions;
        const double gbps = seconds > 0.0 ? bytes / seconds / 1e9 : 0.0;
        const double bytes_per_cycle = frequency_hz > 0.0 && seconds > 0.0
            ? bytes / (seconds * frequency_hz)
            : 0.0;
        ostringstream rate, per_second;
        rate << fixed << setprecision(3) << bytes_per_cycle << " Byte/Cycle";
        per_second << fixed << setprecision(3) << gbps << " GB/s";
        vector<string> row(table.getCol());
        row[0] = "L" + to_string(level) + " Cache";
        row[1] = "rvv-vle8.v";
        row[2] = rate.str();
        row[3] = cpufb::format_cache_capacity(cache.bytes);
        row[4] = cpufb::format_cache_capacity(benchmark.bytes);
        row[5] = cache.source;
        row[6] = per_second.str();
        row[7] = "clock/frequency estimate";
        table.addOneItem(row);
        free_stream_buffers(benchmark);
    }
}

static void add_riscv64_multiple_issue(tpool_t *tm, int cpu, Table &table)
{
    const size_t vector_bytes = riscv_vector_length_bytes();
    const cpufb::CacheLevelInfo l1 = cpufb::detect_data_cache_level(cpu, 1);
    if (vector_bytes == 0 || l1.bytes == 0 || freq.empty() || freq[0] <= 0.0)
        return;
    StreamBenchmark benchmark;
    benchmark.bytes = max<size_t>(vector_bytes, l1.bytes / 2);
    benchmark.bytes -= benchmark.bytes % vector_bytes;
    benchmark.repetitions = max<int64_t>(
        1, static_cast<int64_t>((128ULL * 1024 * 1024) / benchmark.bytes));
    benchmark.mixed = true;
    if (!allocate_stream_buffers(benchmark, tm->thread_num)) {
        free_stream_buffers(benchmark);
        return;
    }
    const double seconds = median_stream_seconds(tm, benchmark);
    const double vector_iterations = static_cast<double>(benchmark.bytes) /
        vector_bytes * benchmark.repetitions;
    const double ipc = seconds > 0.0
        ? 2.0 * vector_iterations / (seconds * freq[0] * 1e9)
        : 0.0;
    vector<string> row(table.getCol());
    row[0] = "RVV_MULTI_ISSUE";
    row[1] = "vle32.v/vfmacc.vv";
    ostringstream value;
    value << fixed << setprecision(4) << ipc << " IPC";
    row[2] = value.str();
    table.addOneItem(row);
    free_stream_buffers(benchmark);
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
        init_table(tables);

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
            add_riscv64_load_rows(tm, set_of_threads[0], *tables[1]);
        if (should_run_test(filter, "multi_issue"))
            add_riscv64_multiple_issue(tm, set_of_threads[0], *tables[4]);
#endif

        // traverse task list

        for (i = 0; i < static_cast<int>(bm_list.size()); i++) {
            if (catalog[i].is_latency) continue;
            if (!should_run_benchmark(filter, bm_list[i].isa, bm_list[i].dim))
                continue;
            if (bm_list[i].dim.find("OPS") != string::npos) {
                int64_t latency = 0;
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
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --idle_time parameter.\n");
        fprintf(
            stderr, "Usage: %s --thread_pool=[xxx] --idle_time=yyy\n", argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr,
            "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr,
            "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "       %s --list-categories | --list-instructions\n",
            argv[0]);
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
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
