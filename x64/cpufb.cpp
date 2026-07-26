#include "table.hpp"
#include "thread_pool.hpp"
#include "cli.hpp"
// #include "./kernel/compute.hpp"
#include<compute.hpp>
#include<load.hpp>
#include<memory_bandwidth.hpp>
#include<frequency.hpp>
#include<common.hpp>
#include<multiple_issue.hpp>
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
using namespace cpufb_cli;
extern vector<double> freq;
static struct CacheData cache_size;


#define LOOP_INCREASE 256
#define LOOP_DECREASE 32

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
    for (i = 0; i < 14; i++)
    {
        __tilecfg.reserved_0[i] = 0;
    }
    for (i = 0; i < 8; i++)
    {
        __tilecfg.colsb[i] = 64;
        __tilecfg.rows[i] = 16;
    }
    for (; i < 16; i++)
    {
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
    return syscall(SYS_arch_prctl, ARCH_REQ_XCOMP_PERM,
        XFEATURE_XTILEDATA) == 0;
#else
    return false;
#endif
}
#endif

typedef struct
{
    std::string isa;
    std::string type;
    std::string dim;
    int64_t loop_time;
    int64_t comp_pl; // Mathematical/element operations per outer asm loop.
    int64_t inst_pl; // Benchmarked instructions per outer asm loop.
    void *params;
    void (*bench)(int64_t, void*);
} cpubm_t;
typedef struct
{
    float* cache_data;
    int inner_loop;
    int loop_time;
    void (*bench)(float*, int, int64_t);
} cache_bm_t;
static vector<cpubm_t> bm_list;
static double g_latency = 0.0;

static BenchmarkCatalog build_benchmark_catalog()
{
    BenchmarkCatalog catalog;
    catalog.reserve(bm_list.size());
    for (const cpubm_t &item : bm_list)
        catalog.push_back(BenchmarkInfo(item.isa, item.type, item.dim));
    return catalog;
}

typedef struct
{
    double perf;
    double ipc;
} ComputeResult;

// static double get_time(struct timespec *start,
//     struct timespec *end)
// {
//     return end->tv_sec - start->tv_sec +
//         (end->tv_nsec - start->tv_nsec) * 1e-9;
// }

static void reg_new_isa(std::string isa,
    std::string type,
    std::string dim,
    int64_t loop_time,
    int64_t comp_pl,
    void *params,
    void (*bench)(int64_t, void*),
    int64_t inst_pl = 16)
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

    bm_list.push_back(new_one);
}

static void thread_func(void *params)
{
    cpubm_t *bm = (cpubm_t*)params;
    if (bm->params)
    {
        bm->bench(bm->loop_time, bm->params);
    }
    else
    {
        bm->bench(bm->loop_time, NULL);
    }
}
static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t*)params;
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    struct timespec start, end;
    cpubm_t warmup = item;
    warmup.loop_time = max<int64_t>(1, item.loop_time / LOOP_DECREASE);
    tpool_run_all(tm, thread_func, (void*)&warmup, &start, &end);

    ComputeResult result = {0.0, 0.0};
    if (!tpool_run_all(tm, thread_func, (void*)&item, &start, &end)) return result;
    double time_used = get_time(&start, &end);
    if (time_used <= 0.0) return result;
    result.perf = item.loop_time * item.comp_pl * tm->thread_num / time_used;
    if (!freq.empty() && freq[0] > 0.0)
        result.ipc = item.loop_time * item.inst_pl / time_used / freq[0] / 1e9;
    return result;
}

static string format_latency_cycles(double latency)
{
    stringstream ss;
    ss << fixed << setprecision(2) << latency;
    return ss.str();
}

static void cpubm_x64_one(tpool_t *tm, cpubm_t &item, Table &table)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    if (is_latency_benchmark(item.type)) {
        g_latency = result.ipc > 0.0 ? 1.0 / result.ipc : 0.0;
        return;
    }

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_perf_value(result.perf, item.dim);
    cont[3] = to_string(result.ipc);
    cont[4] = g_latency > 0.0 ? format_latency_cycles(g_latency) : "-";
    g_latency = 0.0;
    table.addOneItem(cont);
}

static void cpubm_x64_load(cpubm_t &item, Table &table)
{
    double perf = 0;

    vector<string> cont;
    cont.resize(table.getCol());

    double data_size = 0.0;
    bool is_l1 = item.isa == "L1 Cache" ||
        (item.isa == "--------" && item.comp_pl == 32LL);

    if (is_l1){
        data_size = cache_size.test_L1;
        cont[3] = to_string(cache_size.theory_L1) + " KB";
    } else {
        data_size = cache_size.test_L2;
        cont[3] = to_string(cache_size.theory_L2) + " KB";
    }

    perf = get_bandwith(item.loop_time, data_size, item.type);

    stringstream ss1;

    ss1 << setprecision(5) << perf << " " << item.dim;

    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss1.str();
    cont[4] = to_string(static_cast<int64_t>(data_size)) + " KB";
    table.addOneItem(cont);
}


static void cpubm_x64_cache(std::vector<int> &set_of_threads,Table &table)
{
    vector<string> cont;
    cont.resize(table.getCol());
    get_cacheline(&cache_size, set_of_threads[0]);
    // cout << "get cacheline" << endl;
    get_multiway(&cache_size, set_of_threads[0]);
    // cout << "get multiway" << endl;
    get_cachesize(&cache_size, set_of_threads[0]);
    if (cache_size.theory_L1 > 0 &&
        (cache_size.test_L1 <= 0 ||
         cache_size.test_L1 < cache_size.theory_L1 / 4 ||
         cache_size.test_L1 > cache_size.theory_L1 * 4))
        cache_size.test_L1 = cache_size.theory_L1;
    if (cache_size.theory_L2 > 0 &&
        (cache_size.test_L2 <= cache_size.test_L1 ||
         cache_size.test_L2 < cache_size.theory_L2 / 4 ||
         cache_size.test_L2 > cache_size.theory_L2 * 4))
        cache_size.test_L2 = cache_size.theory_L2;
    // cout << "get cachesize" << endl;
    cont[0] = "L1 ways of associativity";
    cont[1] = to_string(cache_size.theory_way);
    cont[2] = to_string(cache_size.test_way);
    table.addOneItem(cont);
    cont[0] = "cacheline size";
    cont[1] = to_string(cache_size.theory_cacheline) + " B";
    cont[2] = to_string(cache_size.test_cacheline) + " B";
    table.addOneItem(cont);
    cont[0] = "L1 cache size";
    cont[1] = to_string(cache_size.theory_L1) + " KB";
    cont[2] = to_string(cache_size.test_L1) + " KB";
    table.addOneItem(cont);
    cont[0] = "L2 cache size";
    cont[1] = to_string(cache_size.theory_L2) + " KB";
    cont[2] = to_string(cache_size.test_L2) + " KB";
    table.addOneItem(cont);
    return;
}

static void cpubm_x64_multiple_issue(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{

    struct timespec start, end;
    double time_used, perf;
    cache_bm_t bm;
    int size = 1024;
    float* cache_data = (float*)malloc(1024);
    //Preventing Compiler Optimization
    for (int i = 0;i < size / sizeof(float); i++){
        cache_data[i] = i;
    }
    int inner_loop = 1024;
    bm.bench = multiple_issue;
    if (item.type.find("ymm") != string::npos) bm.bench = multiple_issue_avx;
    else if (item.type.find("zmm") != string::npos) bm.bench = multiple_issue_avx512;
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
    perf = (double)item.loop_time * (inner_loop * item.comp_pl + 4)/
        (time_used * freq[0] * 1e9);
    stringstream ss;

    ss << setprecision(5) << perf;

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
    ti[3] = "Instr/TSC Cycle";
    ti[4] = "Latency(TSC cyc)";
    tables[0]->setColumnNum(ti.size());
    tables[0]->addOneItem(ti);

    ti.resize(5);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwidth";
    ti[3] = "Theory Size";
    ti[4] = "Test Size";
    tables[1]->setColumnNum(ti.size());
    tables[1]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Theory";
    ti[2] = "Test";
    tables[2]->setColumnNum(ti.size());
    tables[2]->addOneItem(ti);

    ti.resize(6);
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "TSC Freq";
    ti[3] = "Instr/TSC(FSU32)";
    ti[4] = "Instr/TSC(FSU64)";
    ti[5] = "Instr/TSC(LSU ldr)";
    tables[3]->setColumnNum(ti.size());
    tables[3]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Core Instruction";
    ti[2] = "Instr/TSC Cycle";
    tables[4]->setColumnNum(ti.size());
    tables[4]->addOneItem(ti);
}


static bool prepare_instruction_sweep(const vector<int> &threads,
    int,
    void *)
{
    Table freq_table;
    vector<string> freq_head(6);
    freq_head[0] = "Core ID";
    freq_head[1] = "Theory Freq";
    freq_head[2] = "TSC Freq";
    freq_head[3] = "Instr/TSC(FSU32)";
    freq_head[4] = "Instr/TSC(FSU64)";
    freq_head[5] = "Instr/TSC(LSU ldr)";
    freq_table.setColumnNum(freq_head.size());
    freq_table.addOneItem(freq_head);
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
        ComputeResult latency_result = cpubm_run_compute(tm, latency_item);
        if (latency_result.ipc > 0.0)
            sample.latency = format_latency_cycles(1.0 / latency_result.ipc);
    }
    cpubm_t selected = bm_list[benchmark_index];
    ComputeResult result = cpubm_run_compute(tm, selected);
    sample.performance = result.perf;
    sample.ipc = result.ipc;
    tpool_destroy(tm);
    return true;
}

static bool cpubm_do_instruction_sweep(vector<int> &set_of_threads,
    uint32_t idle_time, const string &instruction, const SaveOptions &save_options)
{
    SweepConfig config;
    config.ipc_column = "Instr/TSC Cycle";
    config.latency_column = "Latency(TSC cyc)";
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

static bool cpubm_do_bench(vector<int> &set_of_threads, uint32_t idle_time,
    const BenchmarkFilter &filter, const SaveOptions &save_options)
{
    if (bm_list.empty()) return false;
    printf("Number Threads: %zu\nThread Pool Binding:", set_of_threads.size());
    for (size_t i = 0; i < set_of_threads.size(); ++i) printf(" %d", set_of_threads[i]);
    printf("\n");

    vector<Table*> tables;
    init_table(tables);
    if (benchmark_needs_freq(filter)) get_cpu_freq(set_of_threads, *tables[3]);
    if (should_run_test(filter, "cache"))
        cpubm_x64_cache(set_of_threads, *tables[2]);
    else if (should_run_test(filter, "load"))
        get_theory_cache(&cache_size, set_of_threads[0]);

    tpool_t *tm = tpool_create(set_of_threads);
    for (size_t i = 0; i < bm_list.size(); ++i) {
        if (!should_run_benchmark(filter, bm_list[i].isa, bm_list[i].dim))
            continue;
        sleep(idle_time);
        if (bm_list[i].dim.find("OPS") != string::npos)
            cpubm_x64_one(tm, bm_list[i], *tables[0]);
        else if (bm_list[i].dim.find("Byte/") != string::npos)
            cpubm_x64_load(bm_list[i], *tables[1]);
        else if (bm_list[i].dim.find("IPC") != string::npos)
            cpubm_x64_multiple_issue(tm, bm_list[i], *tables[4]);
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
    const bool amx_enabled = runtime_features.amx_tile &&
        request_tile_data_permission();
    if (amx_enabled) init_tile_cfg();
#endif

#ifdef _AVX2_
    if (runtime_features.avx2) {
    reg_new_isa("AVX2", "ADD(s32,s32)_latency", "OPS",
        0x4000000LL, 128LL, NULL, avx2_add_s32_latency);
    reg_new_isa("AVX2", "ADD(s32,s32)", "OPS",
        0x4000000LL, 128LL, NULL, avx2_add_s32);
    reg_new_isa("AVX2", "MUL(s32,s32)_latency", "OPS",
        0x4000000LL, 128LL, NULL, avx2_mul_s32_latency);
    reg_new_isa("AVX2", "MUL(s32,s32)", "OPS",
        0x4000000LL, 128LL, NULL, avx2_mul_s32);
    }
#endif

#ifdef _AVX512_IFMA_
    if (runtime_features.avx512_ifma) {
    reg_new_isa("AVX512_IFMA", "MADD52(u64,u52,u52)_latency", "OPS",
        0x4000000LL, 256LL, NULL, avx512_ifma_madd52_u64_latency);
    reg_new_isa("AVX512_IFMA", "MADD52(u64,u52,u52)", "OPS",
        0x4000000LL, 256LL, NULL, avx512_ifma_madd52_u64);
    }
#endif

#ifdef _AVX512_VBMI_
    if (runtime_features.avx512_vbmi) {
    reg_new_isa("AVX512_VBMI", "PERMB(u8)_latency", "OPS",
        0x4000000LL, 1024LL, NULL, avx512_vbmi_permb_u8_latency);
    reg_new_isa("AVX512_VBMI", "PERMB(u8)", "OPS",
        0x4000000LL, 1024LL, NULL, avx512_vbmi_permb_u8);
    }
#endif

#ifdef _AVX512_VPOPCNTDQ_
    if (runtime_features.avx512_vpopcntdq) {
    reg_new_isa("AVX512_VPOPCNTDQ", "POPCNT(s32)_latency", "OPS",
        0x4000000LL, 256LL, NULL, avx512_vpopcnt_s32_latency);
    reg_new_isa("AVX512_VPOPCNTDQ", "POPCNT(s32)", "OPS",
        0x4000000LL, 256LL, NULL, avx512_vpopcnt_s32);
    }
#endif

#ifdef _AMX_INT8_
    if (amx_enabled && runtime_features.amx_int8) {
    reg_new_isa("AMX_INT8", "MM(s32,s8,s8)_latency", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32s8s8_latency, 4);
    reg_new_isa("AMX_INT8", "MM(s32,s8,s8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32s8s8, 4);
    reg_new_isa("AMX_INT8", "MM(s32,s8,u8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32s8u8, 4);
    reg_new_isa("AMX_INT8", "MM(s32,u8,s8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32u8s8, 4);
    reg_new_isa("AMX_INT8", "MM(s32,u8,u8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32u8u8, 4);
    }
#endif

#ifdef _AMX_BF16_
    if (amx_enabled && runtime_features.amx_bf16) {
    reg_new_isa("AMX_BF16", "MM(f32,bf16,bf16)_latency", "FLOPS",
        0x2500000LL, 65536LL, &__tilecfg, amx_bf16_mm_f32bf16bf16_latency, 4);
    reg_new_isa("AMX_BF16", "MM(f32,bf16,bf16)", "FLOPS",
        0x2500000LL, 65536LL, &__tilecfg, amx_bf16_mm_f32bf16bf16, 4);
    }
#endif

#ifdef _AVX512_VNNI_
    if (runtime_features.avx512_vnni) {
    reg_new_isa("AVX512_VNNI", "DP4A(s32,u8,s8)_latency", "OPS",
        0x4000000LL, 2048LL, NULL, avx512_vnni_dp4a_s32u8s8_latency);
    reg_new_isa("AVX512_VNNI", "DP4A(s32,u8,s8)", "OPS",
        0x20000000LL, 2048LL, NULL, avx512_vnni_dp4a_s32u8s8);
    reg_new_isa("AVX512_VNNI", "DP2A(s32,s16,s16)_latency", "OPS",
        0x4000000LL, 1024LL, NULL, avx512_vnni_dp2a_s32s16s16_latency);
    reg_new_isa("AVX512_VNNI", "DP2A(s32,s16,s16)", "OPS",
        0x20000000LL, 1024LL, NULL, avx512_vnni_dp2a_s32s16s16);
    }
#endif

#ifdef _AVX_VNNI_
    if (runtime_features.avx_vnni) {
    reg_new_isa("AVX_VNNI", "DP4A(s32,u8,s8)_latency", "OPS",
        0x4000000LL, 1024LL, NULL, avx_vnni_dp4a_s32u8s8_latency);
    reg_new_isa("AVX_VNNI", "DP4A(s32,u8,s8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_dp4a_s32u8s8);
    reg_new_isa("AVX_VNNI", "DP2A(s32,s16,s16)_latency", "OPS",
        0x4000000LL, 512LL, NULL, avx_vnni_dp2a_s32s16s16_latency);
    reg_new_isa("AVX_VNNI", "DP2A(s32,s16,s16)", "OPS",
        0x20000000LL, 512LL, NULL, avx_vnni_dp2a_s32s16s16);
    }
#endif

#ifdef _AVX_VNNI_INT8_
    if (runtime_features.avx_vnni_int8) {
    reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,s8,s8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_int8_dp4a_s32s8s8);
    reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,s8,u8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_int8_dp4a_s32s8u8);
    reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,u8,u8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_int8_dp4a_s32u8u8);
    }
#endif

#ifdef _AVX512_BF16_
    if (runtime_features.avx512_bf16) {
    reg_new_isa("AVX512_BF16", "DP2A(f32,bf16,bf16)_latency", "FLOPS",
        0x4000000LL, 1024LL, NULL, avx512_bf16_dp2a_f32bf16bf16_latency);
    reg_new_isa("AVX512_BF16", "DP2A(f32,bf16,bf16)", "FLOPS",
        0x20000000LL, 1024LL, NULL, avx512_bf16_dp2a_f32bf16bf16);
    }
#endif

#ifdef _AVX512_FP16_
    if (runtime_features.avx512_fp16) {
    reg_new_isa("AVX512_FP16", "FMA(f16,f16,f16)_latency", "FLOPS",
        0x4000000LL, 1024LL, NULL, avx512_fp16_fma_f16f16f16_latency);
    reg_new_isa("AVX512_FP16", "FMA(f16,f16,f16)", "FLOPS",
        0x20000000LL, 1024LL, NULL, avx512_fp16_fma_f16f16f16);
    }
#endif

#ifdef _AVX512F_
    if (runtime_features.avx512f) {
    reg_new_isa("AVX512F", "FMA(f32,f32,f32)_latency", "FLOPS",
        0x4000000LL, 512LL, NULL, avx512f_fma_f32f32f32_latency);
    reg_new_isa("AVX512F", "FMA(f32,f32,f32)", "FLOPS",
        0x20000000LL, 512LL, NULL, avx512f_fma_f32f32f32);
    reg_new_isa("AVX512F", "FMA(f64,f64,f64)_latency", "FLOPS",
        0x4000000LL, 256LL, NULL, avx512f_fma_f64f64f64_latency);
    reg_new_isa("AVX512F", "FMA(f64,f64,f64)", "FLOPS",
        0x20000000LL, 256LL, NULL, avx512f_fma_f64f64f64);
    }
#endif

#ifdef _FMA_
    if (runtime_features.fma) {
    reg_new_isa("FMA", "FMA(f32,f32,f32)_latency", "FLOPS",
        0x4000000LL, 256LL, NULL, fma_f32f32f32_latency);
    reg_new_isa("FMA", "FMA(f32,f32,f32)", "FLOPS",
        0x20000000LL, 256LL, NULL, fma_f32f32f32);
    reg_new_isa("FMA", "FMA(f64,f64,f64)_latency", "FLOPS",
        0x4000000LL, 128LL, NULL, fma_f64f64f64_latency);
    reg_new_isa("FMA", "FMA(f64,f64,f64)", "FLOPS",
        0x20000000LL, 128LL, NULL, fma_f64f64f64);
    }
#endif

#ifdef _AVX_
    if (runtime_features.avx) {
    reg_new_isa("AVX", "ADD(f32,f32)_latency", "FLOPS", 0x4000000LL, 128LL, NULL, avx_add_f32_latency);
    reg_new_isa("AVX", "ADD(f32,f32)", "FLOPS", 0x4000000LL, 128LL, NULL, avx_add_f32);
    reg_new_isa("AVX", "MUL(f32,f32)_latency", "FLOPS", 0x4000000LL, 128LL, NULL, avx_mul_f32_latency);
    reg_new_isa("AVX", "MUL(f32,f32)", "FLOPS", 0x4000000LL, 128LL, NULL, avx_mul_f32);
    reg_new_isa("AVX", "ADD(f64,f64)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, avx_add_f64_latency);
    reg_new_isa("AVX", "ADD(f64,f64)", "FLOPS", 0x4000000LL, 64LL, NULL, avx_add_f64);
    reg_new_isa("AVX", "MUL(f64,f64)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, avx_mul_f64_latency);
    reg_new_isa("AVX", "MUL(f64,f64)", "FLOPS", 0x4000000LL, 64LL, NULL, avx_mul_f64);
    reg_new_isa("AVX", "ADD(MUL(f32,f32),f32)", "FLOPS",
        0x20000000LL, 128LL, NULL, avx_add_mul_f32f32_f32);
    reg_new_isa("AVX", "ADD(MUL(f64,f64),f64)", "FLOPS",
        0x20000000LL, 64LL, NULL, avx_add_mul_f64f64_f64);
    }
#endif

#ifdef _SSE_
    if (runtime_features.sse) {
    reg_new_isa("SSE", "ADD(f32,f32)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, sse_add_f32_latency);
    reg_new_isa("SSE", "ADD(f32,f32)", "FLOPS", 0x4000000LL, 64LL, NULL, sse_add_f32);
    reg_new_isa("SSE", "MUL(f32,f32)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, sse_mul_f32_latency);
    reg_new_isa("SSE", "MUL(f32,f32)", "FLOPS", 0x4000000LL, 64LL, NULL, sse_mul_f32);
    reg_new_isa("SSE", "ADD(MUL(f32,f32),f32)", "FLOPS",
        0x20000000LL, 64LL, NULL, sse_add_mul_f32f32_f32);
    }
#endif

#ifdef _SSE2_
    if (runtime_features.sse2) {
    reg_new_isa("SSE2", "ADD(f64,f64)_latency", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_add_f64_latency);
    reg_new_isa("SSE2", "ADD(f64,f64)", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_add_f64);
    reg_new_isa("SSE2", "MUL(f64,f64)_latency", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_mul_f64_latency);
    reg_new_isa("SSE2", "MUL(f64,f64)", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_mul_f64);
    reg_new_isa("SSE2", "ADD(MUL(f64,f64),f64)", "FLOPS",
        0x20000000LL, 32LL, NULL, sse2_add_mul_f64f64_f64);
    }
#endif

    if (runtime_features.avx) {
    reg_new_isa("L1 Cache", "vmovups.ymm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
    }
    reg_new_isa(runtime_features.avx ? "--------" : "L1 Cache",
        "movss.scalar(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
    reg_new_isa("--------", "movups.xmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
#ifdef _AVX512F_
    if (runtime_features.avx512f) {
    reg_new_isa("--------", "vmovups.zmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
    }
#endif
    if (runtime_features.avx) {
    reg_new_isa("L2 Cache", "vmovups.ymm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
    }
    reg_new_isa(runtime_features.avx ? "--------" : "L2 Cache",
        "movss.scalar(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
    reg_new_isa("--------", "movups.xmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
#ifdef _AVX512F_
    if (runtime_features.avx512f) {
    reg_new_isa("--------", "vmovups.zmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
    }
#endif
    if (runtime_features.sse) {
    reg_new_isa("MULTI_ISSUE", "ldr/fmla", "IPC",
        0x40000LL, 34LL, NULL, NULL);
    }
#ifdef _FMA_
    if (runtime_features.fma) {
    reg_new_isa("MULTI_ISSUE_AVX", "vmovups/vfmadd.ymm", "IPC",
        0x40000LL, 34LL, NULL, NULL);
    }
#endif
#ifdef _AVX512F_
    if (runtime_features.avx512f && runtime_features.fma) {
    reg_new_isa("MULTI_ISSUE_AVX512", "vmovups/vfmadd.zmm", "IPC",
        0x40000LL, 34LL, NULL, NULL);
    }
#endif
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
    if (!options.thread_pool_set)
    {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --idle_time parameter.\n");
        fprintf(stderr, "Usage: %s --thread_pool=[xxx] [--idle_time=yyy] [--include-test=list] [--exclude-test=list] [--include-isa=list] [--exclude-isa=list]\n", argv[0]);
        fprintf(stderr, "       %s --thread_pool=[xxx] --sweep-instruction='Core Computation'\n", argv[0]);
        fprintf(stderr, "       %s --thread_pool=[core] --memory-bandwidth [--memory-size-mib=1024] [--memory-repetitions=5]\n", argv[0]);
        fprintf(stderr, "       %s --list-categories | --list-instructions\n", argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr, "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        return 1;
    }

    if (options.thread_pool.empty()) {
        fprintf(stderr, "Error: --thread_pool must contain at least one CPU.\n");
        return 1;
    }
    if (!finalize_save_options(options.save)) return 1;
    if (!validate_memory_bandwidth_options(options, true)) return 1;
    if (options.memory_bandwidth)
        return run_x64_memory_bandwidth(options) ? 0 : 1;

    cpufb_register_isa();
    BenchmarkCatalog catalog = build_benchmark_catalog();
    if (!validate_benchmark_filter(options.filter, catalog)) return 1;
    if (!options.sweep_instruction.empty())
        return cpubm_do_instruction_sweep(options.thread_pool,
            options.idle_time,
            options.sweep_instruction,
            options.save) ? 0 : 1;
    return cpubm_do_bench(options.thread_pool,
        options.idle_time,
        options.filter,
        options.save) ? 0 : 1;
}
