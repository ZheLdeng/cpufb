#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <vector>
#include <set>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <utility>

#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "table.hpp"
#include "thread_pool.hpp"
#include "cli.hpp"
#include<load.hpp>
#include<memory_bandwidth.hpp>
#include<compute.hpp>
#include<frequency.hpp>
#include<multiple_issue.hpp>
#include<common.hpp>
#include <cache_topology.hpp>
#include <cmath>

#if defined(__linux__) && !defined(__APPLE__)
#include "runtime_features.hpp"
#endif

#ifdef __APPLE__
#include<amx.hpp>
#endif

using namespace std;
using namespace cpufb_cli;
extern vector<double> freq;
static struct CacheData cache_size;
static int64_t load_pl = 0;
static string load_capacity_source;
static constexpr int kFallbackL1CacheKiB = 64;
static constexpr int kFallbackL2CacheKiB = 1024;
typedef struct
{
    string isa;
    string type;
    string dim;
    int64_t loop_time;
    int64_t comp_pl;
    void*  bench;
    const char* required_feature;
} cpubm_t;

typedef struct
{
    float* cache_data;
    int inner_loop;
    int loop_time;
    void (*bench)(float*, int, int64_t);
} cache_bm_t;

static vector<cpubm_t> bm_list;
static const char* registration_required_feature = "_ASIMD_";

static BenchmarkCatalog build_benchmark_catalog()
{
    BenchmarkCatalog catalog;
    catalog.reserve(bm_list.size());
    for (const cpubm_t &item : bm_list)
        catalog.push_back(BenchmarkInfo(item.isa, item.type, item.dim));
    pair_benchmark_latencies(catalog);
    return catalog;
}

static void require_feature(const char* feature)
{
    registration_required_feature = feature;
}

typedef struct
{
    double perf;
    double ipc;
} ComputeResult;

static void reg_new_isa(string isa,
    string type,
    string dim,
    int64_t loop_time,
    int64_t comp_pl,
    void* bench)
{
    cpubm_t new_one;
    new_one.isa = isa;
    new_one.type = type;
    new_one.dim = dim;
    new_one.loop_time = loop_time;
    new_one.comp_pl = comp_pl;
    new_one.bench = (void *)bench;
    new_one.required_feature = registration_required_feature;

    bm_list.push_back(new_one);
}
#ifdef __linux__
// Core cycles counted around the compute kernel itself, summed over the pool
// workers of one tpool_run_all() generation.  A frequency calibrated once at
// start-up drifts under DVFS and differs per ISA width, so it is only the
// fallback when perf_event_open is denied.
static std::atomic<uint64_t> compute_cycle_sum(0);
static std::atomic<int> compute_cycle_samples(0);
#endif

static void thread_func(void *params)
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#endif
    cpubm_t *bm = (cpubm_t*)params;
#ifdef __linux__
    PerfEventCycle cycle_counter(0, false);
    if (cycle_counter.available()) {
        cycle_counter.start();
        ((void(*)(int64_t))bm->bench)(bm->loop_time);
        cycle_counter.stop();
        const long long cycles = cycle_counter.get_cycle();
        if (cycles > 0) {
            compute_cycle_sum.fetch_add(static_cast<uint64_t>(cycles),
                std::memory_order_relaxed);
            compute_cycle_samples.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
#endif
    ((void(*)(int64_t))bm->bench)(bm->loop_time);
}

static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t*)params;
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static bool cpubm_standalone_warmup(vector<int> &set_of_threads)
{
    for (const cpubm_t &item : bm_list) {
        if (get_benchmark_test_type(item.dim) != "compute") continue;

        cpubm_t warmup_item = item;
        warmup_item.loop_time = min<int64_t>(warmup_item.loop_time, 0x4000LL);

        tpool_t *tm = tpool_create(set_of_threads);
        if (tm == NULL) {
            cerr << "Error: failed to create benchmark thread pool." << endl;
            return false;
        }
        for (int i = 0; i < tm->thread_num; i++) {
            tpool_add_work(tm, thread_func, (void*)&warmup_item);
        }
        tpool_wait(tm);
        tpool_destroy(tm);
        return true;
    }
    return true;
}

// Returns the best wall time for item.loop_time iterations.  cycles_per_core
// receives the smallest per-core PMU cycle count for the same iterations, or
// 0 when hardware cycles were not available on every worker.
static double cpubm_measure_compute_time(tpool_t *tm, cpubm_t &item,
    double &cycles_per_core)
{
    struct timespec start, end;
    cycles_per_core = 0.0;

#ifdef __APPLE__
    constexpr int kBenchRepeats = 5;
#else
    constexpr int kBenchRepeats = 5;
    constexpr double kTargetSeconds = 0.05;
    constexpr int64_t kMaxLoopScale = 1024;
#endif
    double best_time = 1e30;
#ifndef __APPLE__
    // Calibrate each instruction to a long enough measurement window. The
    // old fixed loop count made high-throughput 32-core kernels last only a
    // few milliseconds, so thread wake-up skew dominated the result.
    cpubm_t run_item = item;
    if (!tpool_run_all(tm, thread_func, (void*)&run_item, &start, &end))
        return best_time;
    double probe_time = get_time(&start, &end);
    int64_t loop_scale = 1;
    if (probe_time > 0 && probe_time < kTargetSeconds) {
        loop_scale = static_cast<int64_t>(ceil(kTargetSeconds / probe_time));
        loop_scale = max<int64_t>(1, min<int64_t>(loop_scale, kMaxLoopScale));
    }
    if (item.loop_time > INT64_MAX / loop_scale)
        loop_scale = INT64_MAX / item.loop_time;
    run_item.loop_time = item.loop_time * loop_scale;

    for (int rep = 0; rep < kBenchRepeats; ++rep) {
#ifdef __linux__
        compute_cycle_sum.store(0, std::memory_order_relaxed);
        compute_cycle_samples.store(0, std::memory_order_relaxed);
#endif
        if (!tpool_run_all(tm, thread_func, (void*)&run_item, &start, &end))
            return best_time;
        // Normalize to the registered loop count so existing FLOP/OP
        // accounting remains unchanged.
        double t = get_time(&start, &end) / loop_scale;
        if (t < best_time) best_time = t;
#ifdef __linux__
        if (compute_cycle_samples.load(std::memory_order_relaxed) ==
                static_cast<int>(tm->thread_num)) {
            const double cycles = static_cast<double>(
                compute_cycle_sum.load(std::memory_order_relaxed)) /
                tm->thread_num / loop_scale;
            if (cycles > 0.0 &&
                (cycles_per_core == 0.0 || cycles < cycles_per_core))
                cycles_per_core = cycles;
        }
#endif
    }
#else
     // warm up
    //pthread_set_qos_class_self_np( QOS_CLASS_UTILITY, 0 );
    for (int i = 0; i < tm->thread_num; ++i) {
        // ((void(*)(int64_t))item.bench)(item.loop_time);
        dispatch_group_async(tm->group, tm->queue, ^{((void(*)(int64_t))item.bench)(item.loop_time);});
    }
    dispatch_group_wait(tm->group, DISPATCH_TIME_FOREVER);
    for (int rep = 0; rep < kBenchRepeats; ++rep) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        for (int j = 0; j < tm->thread_num; ++j) {
            // ((void(*)(int64_t))item.bench)(item.loop_time);
            dispatch_group_async(tm->group, tm->queue, ^{((void(*)(int64_t))item.bench)(item.loop_time);});
        }
        dispatch_group_wait(tm->group, DISPATCH_TIME_FOREVER);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        double t = get_time(&start, &end);
        if (t < best_time) best_time = t;
    }
#endif

    return best_time;
}

static int64_t cpubm_scaled_comp_pl(const cpubm_t &item)
{
    int64_t comp_pl = item.comp_pl;
#ifdef _SVE_
    if (item.type.find("sve") != string::npos) {
        comp_pl = comp_pl * load_sve_vector_bytes();
    }
#endif
#ifdef _SME_
    const string &type = item.type;
    bool has_opa = type.find("opa.vv") != string::npos;
    string first_param = has_opa ? type.substr(type.find('('), type.find(',')) : "";

    if (has_opa && first_param.find("32") != string::npos) {
        comp_pl = comp_pl * rdsvl() * rdsvl() / 4 / 4;
        //cout << type << " op = " << item.comp_pl << endl;
    } else if (has_opa && first_param.find("64") != string::npos) {
        comp_pl = comp_pl * rdsvl() * rdsvl() / 8 / 8;
        //cout << type << " op = " << item.comp_pl << endl;
    } else if (has_opa && first_param.find("16") != string::npos) {
        // 16-bit accumulator (SME_F16F16 / SME_B16B16): rows=cols=SVL/2.
        comp_pl = comp_pl * rdsvl() * rdsvl() / 2 / 2;
    } else if (type.find("sme") != string::npos) {
        comp_pl = comp_pl * rdsvl();
        //cout << type << " is sme " << item.comp_pl <<endl;
    }
#endif

    return comp_pl;
}

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    ComputeResult result;
    double cycles_per_core = 0.0;
    double time_used = cpubm_measure_compute_time(tm, item, cycles_per_core);
    int64_t comp_pl = cpubm_scaled_comp_pl(item);

    result.perf = item.loop_time * comp_pl * tm->thread_num / time_used;
    // time_used is the synchronized wall time for one invocation per core.
    // Report estimated IPC per core rather than summing all cores into an
    // aggregate value that grows with the thread count.
    bool frequency_available = !freq.empty() && freq[0] > 0;
    if (cycles_per_core > 0.0)
        result.ipc = item.loop_time * 24 / cycles_per_core;
    else
        result.ipc = frequency_available
            ? item.loop_time * 24 / time_used / freq[0] / 1e9 : 0;
    return result;
}

// Latency stays fractional, as on x86: rounding to whole cycles hides both
// clock-estimate error and genuinely non-integer averages (a 3.5-cycle
// reading is a finding, not a 4).
static double cpubm_arm64_latency(tpool_t *tm, cpubm_t &item)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    return result.ipc > 0.0 ? 1.0 / result.ipc : 0.0;
}

static string format_latency_cycles(double latency)
{
    stringstream ss;
    ss << fixed << setprecision(2) << latency;
    return ss.str();
}

static void cpubm_arm64_one(tpool_t *tm,
    cpubm_t &item,
    double latency,
    Table &table)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    vector<string> cont(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_perf_value(result.perf, item.dim);
    cont[3] = result.ipc > 0 ? to_string(result.ipc) : "-";
    cont[4] = latency > 0 ? format_latency_cycles(latency) : "-";
    table.addOneItem(cont);

    // cout << "test fop end" << endl;
}

static void cpubm_arm_load(tpool_t *tm, cpubm_t &item, Table &table)
{

    vector<string> cont;
    cont.resize(table.getCol());
    //cout << "test load begin" << endl;

    int empirical_capacity = 0;
    if (item.isa == "L1 Cache"){
        load_pl = cache_size.theory_L1 > 0 ?
            cache_size.theory_L1 : cache_size.test_L1;
        empirical_capacity = cache_size.test_L1;
        load_capacity_source = cache_size.theory_L1 > 0 ?
            (cache_size.theory_L1_source.empty() ? "OS topology" :
                cache_size.theory_L1_source) :
            (empirical_capacity > 0 ? "cache probe" : "fallback");
    } else if (item.isa == "L2 Cache"){
        load_pl = cache_size.theory_L2 > 0 ?
            cache_size.theory_L2 : cache_size.test_L2;
        empirical_capacity = cache_size.test_L2;
        load_capacity_source = cache_size.theory_L2 > 0 ?
            (cache_size.theory_L2_source.empty() ? "OS topology" :
                cache_size.theory_L2_source) :
            (empirical_capacity > 0 ? "cache probe" : "fallback");
    }

    const int64_t workset_kib = min<int64_t>(load_pl / 2, 32 * 1024);
    cont[3] = to_string(load_pl) + " KiB";
    cont[4] = to_string(workset_kib) + " KiB";
    cont[5] = load_capacity_source;

    // Always run on the pinned pool workers, also for a single core: the
    // calling thread is only pinned as a side effect of the cache probes, so
    // a load-only run would measure whichever core the scheduler picked.
    const LoadBandwidth bandwidth = get_bandwith(item.loop_time,
        (double)load_pl, item.type, item.bench, tm);

    stringstream ss1, ss2;

    if (bandwidth.bytes_per_cycle > 0)
        ss1 << setprecision(5) << bandwidth.bytes_per_cycle << " " << item.dim;
    else
        ss1 << "-";
    if (bandwidth.gb_per_second > 0) {
        ss2 << setprecision(5) << bandwidth.gb_per_second << " GB/s";
        if (bandwidth.thread_num > 1)
            ss2 << " (" << bandwidth.thread_num << " cores)";
    } else {
        ss2 << "-";
    }

    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss1.str();
    cont[6] = ss2.str();
    cont[7] = bandwidth.cycle_source.empty() ? "-" : bandwidth.cycle_source;

    table.addOneItem(cont);
    //cout << "test load end" << endl;
}

static void prepare_arm_load_cache(std::vector<int> &set_of_threads)
{
    get_cache_capacities(&cache_size, set_of_threads[0]);
    if (cache_size.theory_L1 <= 0 && cache_size.test_L1 <= 0) {
        cache_size.theory_L1 = kFallbackL1CacheKiB;
        cache_size.theory_L1_source = "conservative fallback";
    }
    if (cache_size.theory_L2 <= 0 && cache_size.test_L2 <= 0) {
        cache_size.theory_L2 = kFallbackL2CacheKiB;
        cache_size.theory_L2_source = "conservative fallback";
    }
}

static void probe_arm_cache(std::vector<int> &set_of_threads)
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#endif

    get_cacheline(&cache_size, set_of_threads[0]);
    // cout << "get cacheline" << endl;
    get_multiway(&cache_size, set_of_threads[0]);
    // cout << "get multiway" << endl;
    // L1/L2 capacity comes from the dependent-load latency curve in
    // cpubm_arm_cache(); running the legacy slope probe as well would print a
    // second, conflicting set of "measured" sizes.
}

static bool cpubm_arm_cache(std::vector<int> &set_of_threads,
    Table &table,
    const CliOptions &options)
{
    vector<string> cont;

    cont.resize(table.getCol());
    probe_arm_cache(set_of_threads);

    // One capacity measurement feeds both the summary rows and the curve
    // tables below.  Probe results are printed as measured and only labelled
    // against the OS topology, never replaced by it.
    int cpu_id = set_of_threads[0];
    get_reported_cache_info(&cache_size, cpu_id);
    CacheCurveResult curve = measure_cache_hierarchy(&cache_size, cpu_id);
    auto with_agreement = [](const string &source, int reported, int measured,
        double tolerance) {
        const string verdict =
            cpufb::describe_probe_agreement(reported, measured, tolerance);
        return source.empty() ? verdict : source + "; " + verdict;
    };

    cont[0] = "L1 data cache capacity";
    cont[1] = cache_size.theory_L1 > 0 ?
        to_string(cache_size.theory_L1) + " KiB" : "-";
    cont[2] = cache_size.test_L1 > 0 ?
        to_string(cache_size.test_L1) + " KiB" : "-";
    cont[5] = with_agreement(cache_size.theory_L1_source,
        cache_size.theory_L1, cache_size.test_L1, 1.5);
    table.addOneItem(cont);
    cont[0] = "L2/unified cache capacity";
    cont[1] = cache_size.theory_L2 > 0 ?
        to_string(cache_size.theory_L2) + " KiB" : "-";
    cont[2] = cache_size.test_L2 > 0 ?
        to_string(cache_size.test_L2) + " KiB" : "-";
    cont[5] = with_agreement(cache_size.theory_L2_source,
        cache_size.theory_L2, cache_size.test_L2, 1.5);
    table.addOneItem(cont);
    const cpufb::CacheLevelInfo l3 =
        cpufb::detect_data_cache_level(set_of_threads[0], 3);
    if (l3.bytes > 0) {
        cont[0] = "L3/unified cache capacity";
        cont[1] = cpufb::format_cache_capacity(l3.bytes);
        cont[2] = "-";
        cont[3].clear();
        cont[4].clear();
        cont[5] = l3.source;
        table.addOneItem(cont);
    }
    cont[0] = "L1 ways of associativity";
    cont[1] = cache_size.theory_way > 0 ? to_string(cache_size.theory_way) : "-";
    cont[2] = cache_size.test_way > 0 ? to_string(cache_size.test_way) : "-";
    cont[3].clear();
    cont[4].clear();
    cont[5] = with_agreement("", cache_size.theory_way,
        cache_size.test_way, 1.0);
    table.addOneItem(cont);
    cont[0] = "cacheline size";
    cont[1] = cache_size.theory_cacheline > 0 ?
        to_string(cache_size.theory_cacheline) + " B" : "-";
    cont[2] = cache_size.test_cacheline > 0 ?
        to_string(cache_size.test_cacheline) + " B" : "-";
    cont[5] = with_agreement("", cache_size.theory_cacheline,
        cache_size.test_cacheline, 1.0);
    table.addOneItem(cont);

    // Cache-hierarchy latency curve: emit the dependent-load working-set
    // sweep and the inferred per-level capacity/latency/jump estimates.
    cout << "Cache curve translation mode: " << curve.translation_mode << endl;

    Table curve_table;
    curve_table.setColumnNum(2);
    vector<string> curve_head = {"Working Set", "Dependent-load Latency"};
    curve_table.addOneItem(curve_head);
    for (const auto &point : curve.points) {
        vector<string> row(2);
        if (point.working_set_bytes >= 1024 * 1024) {
            ostringstream size;
            size << fixed << setprecision(2)
                 << point.working_set_bytes / (1024.0 * 1024.0) << " MB";
            row[0] = size.str();
        } else {
            row[0] = to_string(point.working_set_bytes / 1024) + " KB";
        }
        ostringstream latency;
        latency << fixed << setprecision(3) << point.latency_ns << " ns/load";
        row[1] = latency.str();
        curve_table.addOneItem(row);
    }
    curve_table.print();

    Table estimate_table;
    estimate_table.setColumnNum(4);
    vector<string> estimate_head = {"Level", "Measured Capacity", "Latency", "Jump"};
    estimate_table.addOneItem(estimate_head);
    for (const auto &level : curve.levels) {
        if (level.level != "L1" && level.level != "L2") continue;
        vector<string> row(4);
        row[0] = level.level;
        row[1] = level.capacity_bytes >= 1024 * 1024
            ? to_string(level.capacity_bytes / (1024 * 1024)) + " MB"
            : to_string(level.capacity_bytes / 1024) + " KB";
        ostringstream latency, jump;
        latency << fixed << setprecision(3) << level.latency_ns << " ns/load";
        jump << fixed << setprecision(2) << level.jump_ratio << "x";
        row[2] = latency.str();
        row[3] = jump.str();
        estimate_table.addOneItem(row);
    }
    estimate_table.print();

    return append_cache_memory_bandwidth(options, table);
}


static void cpubm_arm_multiple_issue(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{
    //cout << "test multi issue start" << endl;
    struct timespec start, end;
    double time_used, perf;
    cache_bm_t bm;
    int num_threads = tm->thread_num;
    int size = 2048;
    float* cache_data = (float*)malloc(size);
    //Preventing Compiler Optimization
    for (int i = 0;i < size / sizeof(float); i++){
        cache_data[i] = i;
    }
    int inner_loop = 1024;
    // if (item.type.find("sme")) {
    bm.bench = reinterpret_cast<void (*)(float *, int, int64_t)>(item.bench);
    // } else {
    //     bm.bench = multiple_issue;
    // }   
    bm.cache_data = cache_data;
    bm.inner_loop = inner_loop;
    bm.loop_time = item.loop_time;

	// warm up
    tpool_add_work(tm, cache_thread_func, (void*)&bm);
    tpool_wait(tm);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    tpool_add_work(tm, cache_thread_func, (void*)&bm);
    tpool_wait(tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    bool frequency_available = !freq.empty() && freq[0] > 0;
    perf = frequency_available
        ? (double)item.loop_time * (inner_loop * item.comp_pl + 4) /
            (time_used * freq[0] * 1e9)
        : 0;
    stringstream ss;

    if (frequency_available)
        ss << setprecision(5) << perf << " " << item.dim;
    else
        ss << "-";

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
    free(cache_data);
    //cout << "test multi issue end" << endl;
}
// compute: instruction throughput/IPC; load: cache-resident load bandwidth;
// cache: capacity, associativity, and cache-line probes; freq: core frequency.
static void init_table(vector<Table*> &tables)
{
    tables.resize(5);
    for (int  i = 0; i < 5; i++)
    {
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
    ti.resize(9);
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
    ti[7] = "IPC(SVE32)";
    ti[8] = "IPC(SVE64)";
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

static void init_frequency_table(Table &table)
{
    vector<string> ti;

#ifdef _SVE_
    ti.resize(9);
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
    ti[7] = "IPC(SVE32)";
    ti[8] = "IPC(SVE64)";
#endif
    table.setColumnNum(ti.size());
    table.addOneItem(ti);
}

static bool prepare_instruction_sweep(const vector<int> &threads,
    int,
    void *)
{
#ifdef _SVE_
#ifdef __APPLE__
    cout << " SVE : " << load_sve_vector_bytes() << endl;
#else
    if (arm64_runtime_features().sve)
        cout << " SVE : " << load_sve_vector_bytes() << endl;
#endif
#endif
#ifdef _SME_
#ifdef __APPLE__
    cout << "SME : " << rdsvl() * 8 << endl;
#else
    if (arm64_runtime_features().sme)
        cout << "SME : " << rdsvl() * 8 << endl;
#endif
#endif

    Table freq_table;
    init_frequency_table(freq_table);
    vector<int> mutable_threads = threads;
    get_cpu_freq(mutable_threads, freq_table);
    return true;
}

static bool measure_instruction_sweep(const vector<int> &active_threads,
    uint32_t idle_time,
    int benchmark_index,
    int latency_index,
    SweepSample &sample,
    void *)
{
    tpool_t *tm = tpool_create(active_threads);
    if (tm == NULL) return false;
    sleep(idle_time);

    if (latency_index >= 0) {
        cpubm_t latency_item = bm_list[latency_index];
        double latency = cpubm_arm64_latency(tm, latency_item);
        if (latency > 0) sample.latency = format_latency_cycles(latency);
    }

    cpubm_t selected_item = bm_list[benchmark_index];
    ComputeResult result = cpubm_run_compute(tm, selected_item);
    sample.performance = result.perf;
    sample.ipc = result.ipc;
    tpool_destroy(tm);
    return true;
}

static bool cpubm_do_instruction_sweep(vector<int> &set_of_threads,
    uint32_t idle_time,
    const string &instruction,
    const SaveOptions &save_options)
{
    SweepConfig config;
    config.print_banner = true;
    config.include_metadata = true;
    return run_instruction_sweep(set_of_threads,
        idle_time,
        instruction,
        build_benchmark_catalog(),
        save_options,
        config,
        prepare_instruction_sweep,
        measure_instruction_sweep,
        NULL);
}

static bool cpubm_do_bench(vector<int> &set_of_threads,
    uint32_t idle_time,
    const BenchmarkFilter &filter,
    const SaveOptions &save_options,
    const CliOptions &options)
{
    int i;
    const uint32_t bench_limit = options.bench_limit;
    uint32_t benches_run = 0;

    if (bm_list.size() > 0)
    {
        int num_threads = set_of_threads.size();

        printf("Number Threads: %d\n", num_threads);
        printf("Thread Pool Binding:");
        for (i = 0; i < num_threads; i++)
        {
            printf(" %d", set_of_threads[i]);
        }
        printf("\n");
#ifdef _SVE_
#ifdef __APPLE__
        cout << " SVE : " << load_sve_vector_bytes() << endl;
#else
        if (arm64_runtime_features().sve)
            cout << " SVE : " << load_sve_vector_bytes() << endl;
#endif
#endif

#ifdef _SME_
#ifdef __APPLE__
        cout << "SME : " << rdsvl() * 8 << endl;
#else
        if (arm64_runtime_features().sme)
            cout << "SME : " << rdsvl() * 8 << endl;
#endif
#endif
        // set table head
        vector<Table*> tables;
        init_table(tables);
        // cout << "start benchmark" << endl;
        if (should_run_standalone_warmup(filter) &&
            !cpubm_standalone_warmup(set_of_threads)) {
            for (size_t t = 0; t < tables.size(); ++t) delete tables[t];
            return false;
        }
        if (benchmark_needs_freq(filter)) {
            get_cpu_freq(set_of_threads, *tables[3]);
        }
        // exit(0);
        // cout << "get freq" << endl;
        if (should_run_test(filter, "cache") &&
            !cpubm_arm_cache(set_of_threads, *tables[2], options)) {
            for (size_t t = 0; t < tables.size(); ++t) delete tables[t];
            return false;
        }
        if (should_run_test(filter, "load"))
            prepare_arm_load_cache(set_of_threads);
        // set thread pool
        tpool_t *tm;
        tm = tpool_create(set_of_threads);
        if (tm == NULL) {
            cerr << "Error: failed to create benchmark thread pool." << endl;
            for (size_t t = 0; t < tables.size(); ++t) delete tables[t];
            return false;
        }
        BenchmarkCatalog catalog = build_benchmark_catalog();

        // traverse task list
        for (i = 0; i < static_cast<int>(bm_list.size()); i++)
        {
            if (bench_limit > 0 && benches_run >= bench_limit) {
                break;
            }
            // cout << bm_list[i].type << endl;
            if (catalog[i].is_latency) continue;
            if (!should_run_benchmark(filter, bm_list[i].isa, bm_list[i].dim))
                continue;

            if (bm_list[i].dim.find("OPS") != string::npos) {
                double latency = 0;
                if (catalog[i].pair_index >= 0) {
                    sleep(idle_time);
                    latency = cpubm_arm64_latency(
                        tm,
                        bm_list[catalog[i].pair_index]);
                }
                sleep(idle_time);
                cpubm_arm64_one(tm, bm_list[i], latency, *tables[0]);
            } else if (bm_list[i].dim.find("Byte/Cycle") != string::npos) {
                sleep(idle_time);
                cpubm_arm_load(tm, bm_list[i], *tables[1]);
            } else if (bm_list[i].dim.find("IPC") != string::npos) {
                sleep(idle_time);
                cpubm_arm_multiple_issue(tm, bm_list[i], *tables[4]);
            } else {
                cout << "Wrong dimension !" << endl;
                break;
            }
            benches_run++;
        }
        bool save_ok = print_and_save_benchmark_tables(filter,
            save_options,
            tables);
        tpool_destroy(tm);
        return save_ok;
    }
    else
    {
        printf("Sorry, there's no any supported SIMD isa.\n");
        return false;
    }
}

static void cpufb_register_isa()
{
    bm_list.clear();
    require_feature("_ASIMD_");
#ifdef __APPLE__
    constexpr int64_t kComputeLoopTime = 0x40000LL;
    constexpr int64_t kLatencyLoopTime = 0x4000LL;
    constexpr int64_t kLoadLoopTime = 0x40000LL;
    constexpr int64_t kMultiIssueLoopTime = 0x40000LL;
#else
    constexpr int64_t kComputeLoopTime = 0x100000LL;
    constexpr int64_t kLatencyLoopTime = 0x10000LL;
    constexpr int64_t kLoadLoopTime = 0x186A00LL;
    constexpr int64_t kMultiIssueLoopTime = 0x186A00LL;
#endif

#ifdef _I8MM_
    require_feature("_I8MM_");
    reg_new_isa("i8mm", "mmla(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 1536LL, (void*)asimd_mmla_s32s8s8_latency);
    reg_new_isa("i8mm", "mmla(s32,s8,s8)", "OPS",
        kComputeLoopTime, 1536LL, (void*)asimd_mmla_s32s8s8);
    reg_new_isa("i8mm", "mmla(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 1536LL, (void*)asimd_mmla_u32u8u8_latency);
    reg_new_isa("i8mm", "mmla(u32,u8,u8)", "OPS",
        kComputeLoopTime, 1536LL, (void*)asimd_mmla_u32u8u8);
    reg_new_isa("i8mm", "mmla(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 1536LL, (void*)asimd_mmla_s32u8s8_latency);
    reg_new_isa("i8mm", "mmla(s32,u8,s8)", "OPS",
        kComputeLoopTime, 1536LL, (void*)asimd_mmla_s32u8s8);

    reg_new_isa("i8mm", "dp4a.vs(s32,s8,u8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8u8_latency);
    reg_new_isa("i8mm", "dp4a.vs(s32,s8,u8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8u8);
    reg_new_isa("i8mm", "dp4a.vs(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_s32u8s8_latency);
    reg_new_isa("i8mm", "dp4a.vs(s32,u8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_s32u8s8);
    reg_new_isa("i8mm", "dp4a.vv(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vv_s32u8s8_latency);
    reg_new_isa("i8mm", "dp4a.vv(s32,u8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vv_s32u8s8);
#endif

#ifdef _ASIMD_DP_
    require_feature("_ASIMD_DP_");
    reg_new_isa("asimd_dp", "dp4a.vs(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8s8_latency);
    reg_new_isa("asimd_dp", "dp4a.vs(s32,s8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8s8);
    reg_new_isa("asimd_dp", "dp4a.vv(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vv_s32s8s8_latency);
    reg_new_isa("asimd_dp", "dp4a.vv(s32,s8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vv_s32s8s8);
    reg_new_isa("asimd_dp", "dp4a.vs(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_u32u8u8_latency);
    reg_new_isa("asimd_dp", "dp4a.vs(u32,u8,u8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_u32u8u8);
    reg_new_isa("asimd_dp", "dp4a.vv(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vv_u32u8u8_latency);
    reg_new_isa("asimd_dp", "dp4a.vv(u32,u8,u8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vv_u32u8u8);
#endif

#ifdef _BF16_
    require_feature("_BF16_");
    reg_new_isa("bf16", "mmla(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 768LL, (void*)asimd_mmla_fp32bf16bf16_latency);
    reg_new_isa("bf16", "mmla(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 768LL, (void*)asimd_mmla_fp32bf16bf16);
    reg_new_isa("bf16", "dp2a.vs(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_dp2a_vs_fp32bf16bf16_latency);
    reg_new_isa("bf16", "dp2a.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_dp2a_vs_fp32bf16bf16);
    reg_new_isa("bf16", "dp2a.vv(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_dp2a_vv_fp32bf16bf16_latency);
    reg_new_isa("bf16", "dp2a.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_dp2a_vv_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalb(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalb_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalb(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalb_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalt(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalt_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalt(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalt_fp32bf16bf16);
    // By-element (lane / laneq) variants. FLOPS coefficient matches the
    // vector form: 24 instr * 4 dest lanes * 2 (FMA) = 192.
    reg_new_isa("bf16", "bfmlalb.lane(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalb_lane_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalb.lane(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalb_lane_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalt.lane(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalt_lane_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalt.lane(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalt_lane_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalb.laneq(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalb_laneq_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalb.laneq(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalb_laneq_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalt.laneq(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalt_laneq_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalt.laneq(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalt_laneq_fp32bf16bf16);
#endif

#ifdef _FHM_
    require_feature("_FHM_");
    reg_new_isa("FHM", "fmlal.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal_vv_f32f16f16);
    reg_new_isa("FHM", "fmlal.vv(f32,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmlal_vv_f32f16f16_latency);
    reg_new_isa("FHM", "fmlal2.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal2_vv_f32f16f16);
    reg_new_isa("FHM", "fmlal2.vv(f32,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmlal2_vv_f32f16f16_latency);
    reg_new_isa("FHM", "fmlal.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal_vs_f32f16f16);
    reg_new_isa("FHM", "fmlal_pair.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal_pair_vv_f32f16f16);
#endif

#ifdef _ASIMD_HP_
    require_feature("_ASIMD_HP_");
    reg_new_isa("asimd_hp", "fmla.vs(fp16,fp16,fp16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fmla_vs_fp16fp16fp16);
    reg_new_isa("asimd_hp", "fmla.vv(fp16,fp16,fp16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fmla_vv_fp16fp16fp16);
#endif

#ifdef _ASIMD_
    require_feature("_ASIMD_");
    reg_new_isa("asimd", "fmla.vs(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmla2_vs_f32f32f32);
    reg_new_isa("asimd", "fmla.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmla_vs_f32f32f32);
    reg_new_isa("asimd", "fmla.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmla2_vv_f32f32f32);
    reg_new_isa("asimd", "fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "fmla.vs(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fmla2_vs_f64f64f64);
    reg_new_isa("asimd", "fmla.vs(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmla_vs_f64f64f64);
    reg_new_isa("asimd", "fmla.vv(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fmla2_vv_f64f64f64);
    reg_new_isa("asimd", "fmla.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmla_vv_f64f64f64);
    reg_new_isa("asimd", "hybrid_fp32_mla_6x16", "FLOPS",
        kComputeLoopTime, 768LL, (void*)asimd_hybrid_fp32_mla_6x16);
    // Tier-2 additions: non-FMA paths and negated-FMA chain.
    reg_new_isa("asimd", "fmls.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmls_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fmls.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmls_vv_f32f32f32);
    reg_new_isa("asimd", "fneg+fmla.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fneg_fmla_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fneg+fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fneg_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "fadd.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fadd_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fadd.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fadd_vv_f32f32f32);
    reg_new_isa("asimd", "fmul.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fmul_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fmul.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmul_vv_f32f32f32);
#endif
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("asimd", "sve_fmla.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fmla_vs_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fmla2_vv_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vs(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fmla_vs_f64f64f64);
    reg_new_isa("asimd", "sve_fmla.vv(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fmla2_vv_f64f64f64);
    reg_new_isa("asimd", "sve_fmla.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fmla_vv_f64f64f64);
    // Tier-2 additions: SVE complex FCMLA + reductions.
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#0_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_0_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#0", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_0);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#90_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_90_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#90", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_90);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#180_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_180_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#180", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_180);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#270_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_270_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#270", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_270);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#0_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_0_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#0", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_0);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#90_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_90_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#90", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_90);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#180_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_180_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#180", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_180);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#270_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_270_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#270", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_270);
    // sve_fadda: Pattern D — same kernel registered twice (latency probe +
    // displayed throughput row) so the displayed row carries fadda's own
    // latency rather than leaking it onto the unrelated row that follows.
    reg_new_isa("sve_reduce", "sve_fadda(f32)_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fadda_v_f32);
    reg_new_isa("sve_reduce", "sve_fadda(f32)", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fadda_v_f32);
    reg_new_isa("sve_reduce", "sve_fadda(f64)_latency", "FLOPS",
        kLatencyLoopTime, 3LL, (void*)sve_fadda_v_f64);
    reg_new_isa("sve_reduce", "sve_fadda(f64)", "FLOPS",
        kLatencyLoopTime, 3LL, (void*)sve_fadda_v_f64);
    reg_new_isa("sve_reduce", "sve_faddv(f32)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_faddv_v_f32);
    reg_new_isa("sve_reduce", "sve_faddv(f64)", "FLOPS",
        kComputeLoopTime, 3LL, (void*)sve_faddv_v_f64);
#endif

#ifdef _ASIMD_FCMA_
    require_feature("_ASIMD_FCMA_");
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#0_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_0_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#0", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_0);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#90_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_90_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#90", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_90);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#180_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_180_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#180", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_180);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#270_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_270_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#270", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_270);
    reg_new_isa("asimd_fcma", "fcmla_pair.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_pair_vv_f32f32f32);
  #ifdef _ASIMD_HP_
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#0_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_0_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#0", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_0);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#90_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_90_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#90", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_90);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#180_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_180_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#180", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_180);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#270_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_270_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#270", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_270);
  #endif
#endif

#ifdef _ASIMD_REDUCE_
    require_feature("_ASIMD_REDUCE_");
    reg_new_isa("asimd_reduce", "faddp.vvv(f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_faddp_v_f32_latency);
    reg_new_isa("asimd_reduce", "faddp.vvv(f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_faddp_v_f32);
    reg_new_isa("asimd_reduce", "fmaxv(f32)", "OPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmaxv_v_f32);
    reg_new_isa("asimd_reduce", "saddlv(s8)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_saddlv_v_s8);
    reg_new_isa("asimd_reduce", "smaxv(s32)", "OPS",
        kComputeLoopTime, 96LL, (void*)asimd_smaxv_v_s32);
  #ifdef _ASIMD_HP_
    reg_new_isa("asimd_reduce", "fmaxv(f16)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmaxv_v_f16);
  #endif
#endif

#ifdef _ASIMD_RECIP_
    require_feature("_ASIMD_RECIP_");
    reg_new_isa("asimd_recip", "frecpe+frecps(f32)_latency", "FLOPS",
        kLatencyLoopTime, 144LL, (void*)asimd_frecpe_recps_v_f32_latency);
    reg_new_isa("asimd_recip", "frecpe+frecps(f32)", "FLOPS",
        kComputeLoopTime, 144LL, (void*)asimd_frecpe_recps_v_f32);
    reg_new_isa("asimd_recip", "frsqrte+frsqrts(f32)_latency", "FLOPS",
        kLatencyLoopTime, 144LL, (void*)asimd_frsqrte_rsqrts_v_f32_latency);
    reg_new_isa("asimd_recip", "frsqrte+frsqrts(f32)", "FLOPS",
        kComputeLoopTime, 144LL, (void*)asimd_frsqrte_rsqrts_v_f32);
  #ifdef _ASIMD_HP_
    reg_new_isa("asimd_recip", "frecpe+frecps(f16)_latency", "FLOPS",
        kLatencyLoopTime, 288LL, (void*)asimd_frecpe_recps_v_f16_latency);
    reg_new_isa("asimd_recip", "frecpe+frecps(f16)", "FLOPS",
        kComputeLoopTime, 288LL, (void*)asimd_frecpe_recps_v_f16);
  #endif
#endif

#ifdef _ASIMD_INT_MAC_
    require_feature("_ASIMD_INT_MAC_");
    reg_new_isa("asimd_int_mac", "mla.vs(s32,s32,s32)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_mla_vs_s32s32s32_latency);
    reg_new_isa("asimd_int_mac", "mla.vs(s32,s32,s32)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_mla_vs_s32s32s32);
    reg_new_isa("asimd_int_mac", "mla.vv(s32,s32,s32)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_mla_vv_s32s32s32_latency);
    reg_new_isa("asimd_int_mac", "mla.vv(s32,s32,s32)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_mla_vv_s32s32s32);
    reg_new_isa("asimd_int_mac", "mla.vs(s16,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 384LL, (void*)asimd_mla_vs_s16s16s16_latency);
    reg_new_isa("asimd_int_mac", "mla.vs(s16,s16,s16)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_mla_vs_s16s16s16);
    reg_new_isa("asimd_int_mac", "sqdmlal.vv(s32,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_sqdmlal_vv_s32s16s16_latency);
    reg_new_isa("asimd_int_mac", "sqdmlal.vv(s32,s16,s16)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_sqdmlal_vv_s32s16s16);
    reg_new_isa("asimd_int_mac", "sqdmlal2.vv(s32,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_sqdmlal2_vv_s32s16s16_latency);
    reg_new_isa("asimd_int_mac", "sqdmlal2.vv(s32,s16,s16)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_sqdmlal2_vv_s32s16s16);
#endif

#ifdef _ASIMD_TBL_
    require_feature("_ASIMD_TBL_");
    // tbl/tbx do 16 lookups per .16b instr, so the OPS number doubles as
    // Byte/Cycle (16 lookup-bytes/instr * IPC). The dispatcher routes any
    // "Byte/Cycle" dim into cpubm_arm_load (different kernel ABI), so we
    // only register the OPS form here; convert mentally as needed.
    reg_new_isa("asimd_tbl", "tbl.4table(u8)_latency", "OPS",
        kLatencyLoopTime, 384LL, (void*)asimd_tbl_4table_v_u8_latency);
    reg_new_isa("asimd_tbl", "tbl.4table(u8)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_tbl_4table_v_u8);
    reg_new_isa("asimd_tbl", "tbx.4table(u8)_latency", "OPS",
        kLatencyLoopTime, 384LL, (void*)asimd_tbx_4table_v_u8_latency);
    reg_new_isa("asimd_tbl", "tbx.4table(u8)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_tbx_4table_v_u8);
#endif

#ifdef _SVE_I8MM_
    require_feature("_SVE_I8MM_");
    reg_new_isa("sve_i8mm", "sve_mmla(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sve_mmla_s32s8s8_latency);
    reg_new_isa("sve_i8mm", "sve_mmla(s32,s8,s8)", "OPS",
        kComputeLoopTime, 96LL, (void*)sve_mmla_s32s8s8);
    reg_new_isa("sve_i8mm", "sve_mmla(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sve_mmla_u32u8u8_latency);
    reg_new_isa("sve_i8mm", "sve_mmla(u32,u8,u8)", "OPS",
        kComputeLoopTime, 96LL, (void*)sve_mmla_u32u8u8);
    reg_new_isa("sve_i8mm", "sve_mmla(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sve_mmla_s32u8s8_latency);
    reg_new_isa("sve_i8mm", "sve_mmla(s32,u8,s8)", "OPS",
        kComputeLoopTime, 96LL, (void*)sve_mmla_s32u8s8);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 12LL, (void*)sve_dp4a_vv_s32s8s8_latency);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,s8,s8)", "OPS",
        kComputeLoopTime, 12LL, (void*)sve_dp4a_vv_s32s8s8);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8u8_latency);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,u8)", "OPS",
        kComputeLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8u8);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8s8_latency);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,s8)", "OPS",
        kComputeLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8s8);
#endif

#ifdef _SVE_BF16_
    require_feature("_SVE_BF16_");
    reg_new_isa("sve_bf16", "sve_bfmmla(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sve_bfmmla_f32bf16bf16_latency);
    reg_new_isa("sve_bf16", "sve_bfmmla(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sve_bfmmla_f32bf16bf16);
    reg_new_isa("sve_bf16", "sve_bfdot.vv(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 24LL, (void*)sve_bfdot_vv_f32bf16bf16_latency);
    reg_new_isa("sve_bf16", "sve_bfdot.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_bfdot_vv_f32bf16bf16);
    reg_new_isa("sve_bf16", "sve_bfdot.vs(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 24LL, (void*)sve_bfdot_vs_f32bf16bf16_latency);
    reg_new_isa("sve_bf16", "sve_bfdot.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_bfdot_vs_f32bf16bf16);
#endif

#ifdef _SVE_F32MM_
    require_feature("_SVE_F32MM_");
    // 24 inst * 32 FLOPs/inst (at VL=128b) / 16 (svcntb scaling unit) = 48
    reg_new_isa("sve_f32mm", "sve_fmmla(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sve_fmmla_f32f32f32);
    reg_new_isa("sve_f32mm", "sve_fmmla(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sve_fmmla_f32f32f32_latency);
#endif

#ifdef _SVE_F64MM_
    require_feature("_SVE_F64MM_");
    // 24 inst * 8 FLOPs/inst (at VL=128b) / 16 = 12
    reg_new_isa("sve_f64mm", "sve_fmmla(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fmmla_f64f64f64);
    reg_new_isa("sve_f64mm", "sve_fmmla(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fmmla_f64f64f64_latency);
#endif

#ifdef _SVE_FP16_FMLA_
    require_feature("_SVE_FP16_FMLA_");
    // 24 inst * 16 FLOPs/inst (at VL=128b, 8 fp16 lanes * 2 ops) / 16 = 24
    reg_new_isa("sve_fp16", "sve_fmla.vv(f16,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_fmla_vv_f16f16f16);
    reg_new_isa("sve_fp16", "sve_fmla.vs(f16,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_fmla_vs_f16f16f16);
    reg_new_isa("sve_fp16", "sve_fmla.vv(f16,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 24LL, (void*)sve_fmla_vv_f16f16f16_latency);
#endif

#ifdef _SVE2_
    require_feature("_SVE2_");
    // 24 inst * 16 OPs/inst (at VL=128b, 8 i16 lanes * 2 ops) / 16 = 24
    reg_new_isa("sve2", "sve2_sqrdmlah.vv(s16,s16,s16)", "OPS",
        kComputeLoopTime, 24LL, (void*)sve2_sqrdmlah_vv_s16s16s16);
    reg_new_isa("sve2", "sve2_sqrdmlah.vv(s16,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 24LL, (void*)sve2_sqrdmlah_vv_s16s16s16_latency);
#endif

#ifdef _SME_
    require_feature("_SME_B16F32_");
    reg_new_isa("SME", "sme_bfmopa.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme_bfmopa_vv_f32bf16bf16);
    reg_new_isa("SME", "sme_bfmopa.vv(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)sme_bfmopa2_vv_f32bf16bf16);
    require_feature("_SME_F32F32_");
    reg_new_isa("SME", "sme_fmopa.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sme_fmopa2_vv_f32f32f32);
    reg_new_isa("SME", "sme_fmopa.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa_vv_f32f32f32);
    require_feature("_SME_F16F32_");
    reg_new_isa("SME", "sme_fmopa.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme_fmopa_vv_f32f16f16);
    require_feature("_SME_I8I32_");
    reg_new_isa("SME", "sme_smopa.vv(i32,i8,i8)", "OPS",
        kComputeLoopTime, 192LL, (void*)sme_smopa_vv_i32i8i8);
    reg_new_isa("SME", "sme_umopa.vv(u32,u8,u8)", "OPS",
        kComputeLoopTime, 192LL, (void*)sme_umopa_vv_u32u8u8);
    reg_new_isa("SME", "sme_usmopa.vv(s32,u8,s8)", "OPS",
        kComputeLoopTime, 192LL, (void*)sme_usmopa_vv_s32u8s8);
    reg_new_isa("SME", "sme_sumopa.vv(s32,s8,u8)", "OPS",
        kComputeLoopTime, 192LL, (void*)sme_sumopa_vv_s32s8u8);
    require_feature("_SME_F32F32_");
    reg_new_isa("SME_MULTI_ISSUE", "ldr/fmopa", "IPC",
        0x186A0LL, 40LL, (void*)sme_multiple_issue);
#endif

#ifdef _SME_F16F16_
    require_feature("_SME_F16F16_");
    // f16 acc: rows=cols=SVL/2, per inst = SVL^2/2 FLOPs.
    // Scaling rule (16 branch): comp_pl * SVL^2 / 4. Register 48 (=24*2).
    reg_new_isa("SME_F16F16", "sme_fmopa.vv(f16,f16,f16)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa_vv_f16f16f16);
    reg_new_isa("SME_F16F16", "sme_fmopa.vv(f16,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sme_fmopa2_vv_f16f16f16);
#endif

#ifdef _SME_B16B16_
    require_feature("_SME_B16B16_");
    // Non-widening BFMOPA: bf16 acc, rows=cols=SVL/2, per inst = SVL^2/2 FLOPs.
    // Scaling rule (16 branch): comp_pl * SVL^2 / 4. Register 48 (=24*2).
    // Twice the FLOPs/instruction of the widening BFMOPA in the SME group.
    reg_new_isa("SME_B16B16", "sme_bfmopa.vv(bf16,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_bfmopa_vv_bf16bf16bf16);
    reg_new_isa("SME_B16B16", "sme_bfmopa.vv(bf16,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sme_bfmopa2_vv_bf16bf16bf16);
#endif

#ifdef _SME_I16I32_
    require_feature("_SME_I16I32_");
    // i32 acc, i16 elems: rows=cols=SVL/4, per inst = SVL^2/4 OPs.
    // Scaling rule (32 branch): comp_pl * SVL^2 / 16. Register 96 (=24*4).
    reg_new_isa("SME_I16I32", "sme_smopa.vv(i32,i16,i16)", "OPS",
        kComputeLoopTime, 96LL, (void*)sme_smopa_vv_i32i16i16);
    reg_new_isa("SME_I16I32", "sme_smopa.vv(i32,i16,i16)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sme_smopa2_vv_i32i16i16);
    reg_new_isa("SME_I16I32", "sme_umopa.vv(i32,i16,i16)", "OPS",
        kComputeLoopTime, 96LL, (void*)sme_umopa_vv_i32i16i16);
    reg_new_isa("SME_I16I32", "sme_umopa.vv(i32,i16,i16)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sme_umopa2_vv_i32i16i16);
#endif

#ifdef _SME2_
    require_feature("_SME2_");
    reg_new_isa("SME2", "sme2_bfmlal.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfmlal_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfmlal4_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfmlal_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfmlal4_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfmlal_mvv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfmlal4_mvv_f32bf16bf16);

    reg_new_isa("SME2", "sme2_bfdot.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfdot_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfdot4_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfdot_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfdot4_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfdot_mvv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfdot4_mvv_f32bf16bf16);
    
    // FMLA/FDOT into ZA have no single-vector form: a four-register list
    // assembles to the VGx4 encoding whether or not VGx4 is spelled out, so
    // the rows without the "4" suffix execute four vectors per instruction
    // and are counted as such.  They differ from the fmla4/fdot4 rows only in
    // their overlapping ZA slice selection.
    reg_new_isa("SME2", "sme2_fmla.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla_vs_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla4_vs_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla_vv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla4_vv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.mvv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sme2_fmla2_mvv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.mvv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla_mvv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.mvv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla4_mvv_f32f32f32);

    reg_new_isa("SME2", "sme2_fmlal.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmlal_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal4_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmlal_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal4_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal_mvv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal4_mvv_f32f16f16);
    
    reg_new_isa("SME2", "sme2_fvdot.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 36LL, (void*)sme2_fvdot_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fvdot2.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 72LL, (void*)sme2_fvdot2_vs_f32f16f16);
    
    reg_new_isa("SME2", "sme2_fdot.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot4.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot4_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot_mvv_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot4.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot4_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot4.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot4_mvv_f32f16f16);
#endif


#ifdef _SMEf64_
    require_feature("_SME_F64F64_");
    reg_new_isa("SMEf64", "sme_fmopa2.vv(f64,f64,f64)_latency", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa2_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme_fmopa.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa_vv_f64f64f64);
#endif

#if defined(_SMEf64_) && defined(_SME2_)
    require_feature("_SME2_F64F64_");
    reg_new_isa("SMEf64", "sme2_fmla.vs(f64,f64,f64)_latency", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla2_vs_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.vs(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla_vs_f64f64f64);
    
    reg_new_isa("SMEf64", "sme2_fmla4.vs(f64,f64,f64)", "FLOPS",

        kComputeLoopTime, 24LL, (void*)sme2_fmla4_vs_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.vv(f64,f64,f64)_latency", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla2_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla4.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla4_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.mvv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla_mvv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla4.mvv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla4_mvv_f64f64f64);
#endif
    require_feature("_LDP_");
    reg_new_isa("L1 Cache", "ldp(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_ldp_kernel);
    reg_new_isa("--------", "neon-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1b_kernel);
    reg_new_isa("--------", "neon-ld1h-x4(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1h_kernel);
    reg_new_isa("--------", "neon-ld1h-4x1(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1h_4x1_kernel);
    reg_new_isa("--------", "ldr.q(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_ldrq_4x1_offset_kernel);
    reg_new_isa("--------", "neon-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1w_kernel);
    reg_new_isa("--------", "neon-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1d_kernel);
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("--------", "sve-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_sve_ld1b_kernel);
    reg_new_isa("--------", "sve-ld1h(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_sve_ld1h_kernel);
    reg_new_isa("--------", "sve-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_ld1w_kernel);
    reg_new_isa("--------", "sve-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_sve_ld1d_kernel);
#endif
#ifdef _SME_
    require_feature("_SME_");
    reg_new_isa("--------", "ldrZA(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ldr_kernel);
    reg_new_isa("--------", "ld1wZAV(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wV_kernel);
    reg_new_isa("--------", "ld1wZAH(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wH_kernel);
#endif
#ifdef _SME2_
    require_feature("_SME2_");
    reg_new_isa("--------", "ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1w_kernel);
#endif
    require_feature("_LDP_");
    reg_new_isa("L2 Cache", "ldp(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_ldp_kernel);
    reg_new_isa("--------", "neon-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1b_kernel);
    reg_new_isa("--------", "neon-ld1h-x4(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1h_kernel);
    reg_new_isa("--------", "neon-ld1h-4x1(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1h_4x1_kernel);
    reg_new_isa("--------", "ldr.q(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_ldrq_4x1_offset_kernel);
    reg_new_isa("--------", "neon-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1w_kernel);
    reg_new_isa("--------", "neon-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1d_kernel);
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("--------", "sve-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_sve_ld1b_kernel);
    reg_new_isa("--------", "sve-ld1h(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_sve_ld1h_kernel);
    reg_new_isa("--------", "sve-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_ld1w_kernel);
    reg_new_isa("--------", "sve-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_sve_ld1d_kernel);
#endif
#ifdef _SME_
    require_feature("_SME_");
    reg_new_isa("--------", "ldrZA(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ldr_kernel);   
    reg_new_isa("--------", "ld1wZAV(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wV_kernel);
    reg_new_isa("--------", "ld1wZAH(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wH_kernel);     
#endif
#ifdef _SME2_
    require_feature("_SME2_");
    reg_new_isa("--------", "ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1w_kernel);
#endif
#ifdef __APPLE__
    // reg_new_isa("Apple amx", "fmla.mat(f16,f16,f16)", "FLOPS",
    //     kComputeLoopTime, 16384LL, (void*)fmla16_benchmark_mat);
    // reg_new_isa("Apple amx", "fmla.mat(f32,f32,f32)", "FLOPS",
    //     kComputeLoopTime, 4096LL, (void*)fmla32_benchmark_mat);
    // reg_new_isa("Apple amx", "fmla.mat(f64,f64,f64)", "FLOPS",
    //     kComputeLoopTime, 1024LL, (void*)fmla64_benchmark_mat);

    // reg_new_isa("Apple amx", "fmla.vec(f16,f16,f16)", "FLOPS",
    //     kComputeLoopTime, 512LL, (void*)fmla16_benchmark_vec);
    // reg_new_isa("Apple amx", "fmla.vec(f32,f32,f32)", "FLOPS",
    //     kComputeLoopTime, 256LL, (void*)fmla32_benchmark_vec);
    // reg_new_isa("Apple amx", "fmla.vec(f64,f64,f64)", "FLOPS",
    //     kComputeLoopTime, 128LL, (void*)fmla64_benchmark_vec);

    // reg_new_isa("Apple amx", "mat.mat(i8,i8,i8)", "OPS",
    //     kComputeLoopTime, 65536LL, (void*)matint_i8i8_benchmark);
    // reg_new_isa("Apple amx", "fmla.mat(i8,i16,i16)", "OPS",
    //     kComputeLoopTime, 32768LL, (void*)matint_i8i16_benchmark);
    // reg_new_isa("Apple amx", "fmla.mat(i16,i16,i16)", "OPS",
    //     kComputeLoopTime, 16384LL, (void*)matint_i16i16_benchmark);

    // reg_new_isa("Apple amx", "ldx 1 reg", "Byte/Cycle",
    //     kLoadLoopTime, 32LL, (void*)load_benchmark_1);   
    // reg_new_isa("Apple amx", "ldx 2 reg", "Byte/Cycle",
    //     kLoadLoopTime, 32LL, (void*)load_benchmark_2);
    // reg_new_isa("Apple amx", "ldx 4 reg", "Byte/Cycle",
    //     kLoadLoopTime, 32LL, (void*)load_benchmark_4);    
    
    
#endif
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("SVE_MULTI_ISSUE", "ld1w/fmla", "IPC",
        kMultiIssueLoopTime, 34LL, (void*)sve_multiple_issue);
    reg_new_isa("SVE_ADD_MULTI_ISSUE", "ld1w/fmla+add(5:1)", "IPC",
        kMultiIssueLoopTime, 26LL, (void*)sve_scalar_add_5_1);
    reg_new_isa("SVE_ADD_MULTI_ISSUE", "ld1w/fmla+add(5:2)", "IPC",
        kMultiIssueLoopTime, 30LL, (void*)sve_scalar_add_5_2);
    reg_new_isa("SVE_ADD_MULTI_ISSUE", "ld1w/fmla+add(5:3)", "IPC",
        kMultiIssueLoopTime, 34LL, (void*)sve_scalar_add_5_3);
    reg_new_isa("SVE_ADD_MULTI_ISSUE", "ld1w/fmla+add(5:4)", "IPC",
        kMultiIssueLoopTime, 38LL, (void*)sve_scalar_add_5_4);
    reg_new_isa("SVE_ADD_MULTI_ISSUE", "ld1w/fmla+add(5:5)", "IPC",
        kMultiIssueLoopTime, 42LL, (void*)sve_scalar_add_5_5);
    reg_new_isa("SVE_ADD_MULTI_ISSUE", "ld1w/fmla+add(5:6)", "IPC",
        kMultiIssueLoopTime, 46LL, (void*)sve_scalar_add_5_6);
#endif
    require_feature("_ISSUE_");
    reg_new_isa("NEON_MULTI_ISSUE", "ldr/fmla", "IPC",
        kMultiIssueLoopTime, 34LL, (void*)neon_multiple_issue);
    reg_new_isa("MULTI_ISSUE", "ldr/fmla", "IPC",
        kMultiIssueLoopTime, 50LL, (void*)multiple_issue);

#if defined(__linux__) && !defined(__APPLE__)
    const Arm64RuntimeFeatures &features = arm64_runtime_features();
    bm_list.erase(remove_if(bm_list.begin(), bm_list.end(),
        [&](const cpubm_t &item) {
            return !features.supports(item.required_feature);
        }),
        bm_list.end());

    cout << "Runtime ARM64 ISA:";
    for (const string &token : features.runnable_tokens()) cout << ' ' << token;
    cout << endl;
#endif
}

static void scale_benchmark_loops(uint32_t loop_scale)
{
    if (loop_scale <= 1) {
        return;
    }

    for (auto &bm : bm_list) {
        bm.loop_time = bm.loop_time / loop_scale;
        if (bm.loop_time < 1) {
            bm.loop_time = 1;
        }
    }
}

int main(int argc, char *argv[])
{
    CliOptions options;
    if (!parse_cli_options(argc, argv, options)) return 1;

    if (options.list_categories || options.list_instructions)
    {
        cpufb_register_isa();
        BenchmarkCatalog catalog = build_benchmark_catalog();
        if (options.list_categories) print_benchmark_categories(catalog);
        if (options.list_instructions) print_benchmark_instructions(catalog);
        return 0;
    }

    if (!options.thread_pool_set)
    {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --mode, --idle_time, --loop_scale and --bench_limit.\n");
        fprintf(stderr, "Usage: %s --thread_pool=[xxx] --idle_time=yyy [--mode=all|cache|compute] [--loop_scale=zzz] [--bench_limit=nnn] [--include-isa=list] [--exclude-isa=list] [--include-test=list] [--exclude-test=list]\n", argv[0]);
        fprintf(stderr, "       %s --thread_pool=[xxx] --sweep-instruction='Core Computation'\n", argv[0]);
        fprintf(stderr, "       %s --thread_pool=[cores] --memory-bandwidth [--memory-size-mib=N; default=auto per stream] [--memory-repetitions=5]\n", argv[0]);
        fprintf(stderr, "       %s --list-categories | --list-instructions\n", argv[0]);
        fprintf(stderr, "       add --save=path [--save-format=txt|csv] to write compact output.\n");
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr, "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "test list supports compute,load,cache,freq,multi_issue (cache=probe+L3/memory stream; load=L1/L2 instruction bandwidth).\n");
        fprintf(stderr, "mode selects cache hierarchy, compute benchmarks, or both; the default is all.\n");
        fprintf(stderr, "loop_scale divides benchmark loop counts for quick smoke runs; default is 1.\n");
        fprintf(stderr, "bench_limit limits the number of registered benchmarks to run; default is 0 for all.\n");
        fprintf(stderr, "isa list supports detected ISA names such as asimd,bf16,sve,SME2.\n");
        fprintf(stderr, "Use --list-categories to list available test and ISA categories.\n");
        fprintf(stderr, "Use --list-instructions to list valid sweep instruction names.\n");
        fprintf(stderr, "save format defaults to csv for .csv paths, otherwise txt.\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        exit(0);
    }

    if (options.thread_pool.empty()) {
        fprintf(stderr, "Error: --thread_pool must contain at least one CPU.\n");
        return 1;
    }
    if (!finalize_save_options(options.save)) return 1;
    if (!validate_memory_bandwidth_options(options, true)) return 1;
    if (options.memory_bandwidth)
        return run_memory_bandwidth(options) ? 0 : 1;

    cpufb_register_isa();
    scale_benchmark_loops(options.loop_scale);
    BenchmarkCatalog catalog = build_benchmark_catalog();

    if (!validate_benchmark_filter(options.filter, catalog)) return 1;

    if (!options.sweep_instruction.empty()) {
        return cpubm_do_instruction_sweep(options.thread_pool,
            options.idle_time,
            options.sweep_instruction,
            options.save) ? 0 : 1;
    }

    return cpubm_do_bench(options.thread_pool,
        options.idle_time,
        options.filter,
        options.save,
        options) ? 0 : 1;

}
