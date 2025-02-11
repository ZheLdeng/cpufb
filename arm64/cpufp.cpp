#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <fstream>

#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "table.hpp"
#include "thread_pool.hpp"
#include<load.hpp>
#include<compute.hpp>
#include<frequency.hpp>
#include<multiple_issue.hpp>
#include<common.hpp>

#ifdef _SVE_FMLA_
#include <arm_sve.h>
#endif

using namespace std;
extern vector<double> freq;
static struct CacheData cache_size;

typedef struct
{
    string isa;
    string type;
    string dim;
    int64_t loop_time;
    int64_t comp_pl;
    void (*bench)(int64_t);
} cpubm_t;

typedef struct
{
    float* cache_data;
    int inner_loop;
    int loop_time;
    void (*bench)(float*, int, int64_t);
} cache_bm_t;

static vector<cpubm_t> bm_list;

static void reg_new_isa(string isa,
    string type,
    string dim,
    int64_t loop_time,
    int64_t comp_pl,
    void (*bench)(int64_t))
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
    cpubm_t *bm = (cpubm_t*)params;
    bm->bench(bm->loop_time);
}


static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t*)params;
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static void cpubm_arm64_one(tpool_t *tm,
    cpubm_t &item, Table &table)
{
    // cout << "test fop begin" << endl;
    struct timespec start, end;
    double time_used, perf, IPC;
    char perfUnit = 'G';

    int i;
    int num_threads = tm->thread_num;
#ifndef __APPLE__
     // warm up
    for (i = 0; i < tm->thread_num; i++) {
        tpool_add_work(tm, thread_func, (void*)&item);
    }
    tpool_wait(tm);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (i = 0; i < tm->thread_num; i++) {
        tpool_add_work(tm, thread_func, (void*)&item);
    }
    tpool_wait(tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
#else
     // warm up
    for (int i = 0; i < tm->thread_num; ++i) {
        dispatch_group_async(tm->group, tm->queue, ^{item.bench(item.loop_time);});
    }
    dispatch_group_wait(tm->group, DISPATCH_TIME_FOREVER);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (int i = 0; i < tm->thread_num; ++i) {
        dispatch_group_async(tm->group, tm->queue, ^{item.bench(item.loop_time);});
    }
    dispatch_group_wait(tm->group, DISPATCH_TIME_FOREVER);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
#endif


#ifdef _SVE_FMLA_
    if (item.type.find("sve") != string::npos) {
        item.comp_pl = item.comp_pl * svcntb();
    }
#endif
#ifdef _SME_
    auto& type = item.type;
    bool has_opa = type.find("opa.vv") != string::npos;
    string first_param = has_opa ? type.substr(type.find('('), type.find(',')) : "";

    if (has_opa && first_param.find("32") != string::npos) {
        item.comp_pl = item.comp_pl * rdsvl8() * rdsvl8() / 32 / 32;
        cout << type << " op = " << item.comp_pl << endl;
    } else if (has_opa && first_param.find("64") != string::npos) {
        item.comp_pl = item.comp_pl * rdsvl8() * rdsvl8() / 64 / 64;
    } else if (type.find("sme") != string::npos) {
        item.comp_pl = item.comp_pl * rdsvl8() / 8;
        cout << type << " is sme " << endl;
    }
    
    // if (item.type.find("opa.vv.(f32") != string::npos) {
    //     item.comp_pl = item.comp_pl * rdsvl8() * rdsvl8() / 32 / 32 ;
    //     cout << item.type << " op = " << item.comp_pl << endl;
    // } else if ((item.type.find("opa.vv.(f64") != string::npos)) {
    //     item.comp_pl = item.comp_pl * rdsvl8() * rdsvl8() / 64 / 64 ;
    // } else if (item.type.find("sme") != string::npos) {
    //     item.comp_pl = item.comp_pl * rdsvl8() / 8;
    //     cout << item.type << " is sme " << endl;
    // }
    
#endif
    time_used = get_time(&start, &end);
    perf = item.loop_time * item.comp_pl * num_threads /
        time_used;
    if (perf > 1e12)
    {
        perfUnit = 'T';
        perf /= 1e12;
    }
    else
    {
        perf /= 1e9;
    }

    stringstream ss1, ss2;

    ss1 << setprecision(5) << perf << " " << perfUnit << item.dim;
    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss1.str();
    table.addOneItem(cont);
    // cout << "test fop end" << endl;
}

static void cpubm_arm_load(cpubm_t &item, Table &table)
{
    double perf = 0;

    vector<string> cont;
    cont.resize(table.getCol());
    cout << "test load begin" << endl;
    double data_size = 0.0;

    if (item.isa == "L1 Cache"){
        item.comp_pl = cache_size.test_L1;
        cont[3] = to_string(cache_size.theory_L1) + " KB";
    } else {
        item.comp_pl = cache_size.test_L2;
        cont[3] = to_string(cache_size.theory_L2) + " KB";
    }

    perf = get_bandwith(item.loop_time, (double)item.comp_pl, item.type);

    stringstream ss1;

    ss1 << setprecision(5) << perf << " " << item.dim;

    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss1.str();
    cont[4] = to_string(item.comp_pl) + " KB";
    table.addOneItem(cont);
    cout << "test load end" << endl;
}

static void cpubm_arm_cache(std::vector<int> &set_of_threads,Table &table)
{
    vector<string> cont;
    cont.resize(table.getCol());
    get_cacheline(&cache_size, set_of_threads[0]);
    cout << "get cacheline" << endl;
    get_multiway(&cache_size, set_of_threads[0]);
    cout << "get multiway" << endl;
    get_cachesize(&cache_size, set_of_threads[0]);
    cout << "get cachesize" << endl;
    cont[0] = "L1 ways of associativity";
    cont[1] = to_string(cache_size.theory_way);
    cont[2] = to_string(cache_size.test_way);
    table.addOneItem(cont);
    cont[0] = "cacheline size";
    cont[1] = to_string(cache_size.theory_cacheline) + " B";
    cont[2] = to_string(cache_size.test_cacheline) + " B";
    table.addOneItem(cont);
    return;
}


static void cpubm_arm_multiple_issue(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{
    cout << "test multi issue start" << endl;
    struct timespec start, end;
    double time_used, perf;
    cache_bm_t bm;
    int num_threads = tm->thread_num;
    int size = 1024;
    float* cache_data = (float*)malloc(1024);
    //Preventing Compiler Optimization
    for (int i = 0;i < size / sizeof(float); i++){
        cache_data[i] = i;
    }
    int inner_loop = 1024;
    bm.bench = multiple_issue;
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

    ss << setprecision(5) << perf << " " << item.dim;

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
    free(cache_data);
    cout << "test multi issue end" << endl;
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

    ti.resize(3);
    ti[0] = "Instruction Set";
    ti[1] = "Core Computation";
    ti[2] = "Peak Performance";
    tables[0]->setColumnNum(ti.size());
    tables[0]->addOneItem(ti);

    ti.resize(5);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwith";
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

    #ifdef _SVE_FMLA_
    ti.resize(8);
    #else
    ti.resize(6);
    #endif
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "Test Freq";
    ti[3] = "IPC(FSU32)";
    ti[4] = "IPC(FSU64)";
    ti[5] = "IPC(LSU ldr)";
    #ifdef _SVE_FMLA_
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
static void cpubm_do_bench(vector<int> &set_of_threads,
    uint32_t idle_time)
{
    int i;

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
        // set table head
        vector<Table*> tables;
        init_table(tables);
        // cout << "start benchmark" << endl;
        get_cpu_freq(set_of_threads, *tables[3]);
        // cout << "get freq" << endl;
        cpubm_arm_cache(set_of_threads, *tables[2]);
        // set thread pool
        tpool_t *tm;
        tm = tpool_create(set_of_threads);

        // traverse task list
        // cpubm_arm64_one(tm, bm_list[0], table);
        for (i = 1; i < bm_list.size(); i++)
        {
            sleep(idle_time);
            if (bm_list[i].dim.find("OPS") != string::npos) {
                cpubm_arm64_one(tm, bm_list[i], *tables[0]);
            } else if (bm_list[i].dim.find("Byte/Cycle") != string::npos) {
                cpubm_arm_load(bm_list[i], *tables[1]);
            } else if (bm_list[i].dim.find("IPC") != string::npos) {
                cpubm_arm_multiple_issue(tm, bm_list[i], *tables[4]);
            } else {
                cout << "Wrong dimension !" << endl;
                break;
            }
        }
        for (i = 0; i < tables.size(); i++)
            tables[i]->print();
        tpool_destroy(tm);
    }
    else
    {
        printf("Sorry, there's no any supported SIMD isa.\n");
    }
}

static void cpufp_register_isa()
{
#ifdef _I8MM_
    reg_new_isa("i8mm", "mmla(s32,s8,s8)", "OPS",
        0x100000LL, 1536LL, asimd_mmla_s32s8s8);
    reg_new_isa("i8mm", "mmla(u32,u8,u8)", "OPS",
        0x100000LL, 1536LL, asimd_mmla_u32u8u8);
    reg_new_isa("i8mm", "mmla(s32,u8,s8)", "OPS",
        0x100000LL, 1536LL, asimd_mmla_s32u8s8);

    reg_new_isa("i8mm", "dp4a.vs(s32,s8,u8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vs_s32s8u8);
    reg_new_isa("i8mm", "dp4a.vs(s32,u8,s8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vs_s32u8s8);
    reg_new_isa("i8mm", "dp4a.vv(s32,u8,s8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vv_s32u8s8);
#endif

#ifdef _ASIMD_DP_
    reg_new_isa("asimd_dp", "dp4a.vs(s32,s8,s8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vs_s32s8s8);
    reg_new_isa("asimd_dp", "dp4a.vv(s32,s8,s8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vv_s32s8s8);
    reg_new_isa("asimd_dp", "dp4a.vs(u32,u8,u8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vs_u32u8u8);
    reg_new_isa("asimd_dp", "dp4a.vv(u32,u8,u8)", "OPS",
        0x100000LL, 768LL, asimd_dp4a_vv_u32u8u8);
#endif

#ifdef _BF16_
    reg_new_isa("bf16", "mmla(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 768LL, asimd_mmla_fp32bf16bf16);
    reg_new_isa("bf16", "dp2a.vs(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 384LL, asimd_dp2a_vs_fp32bf16bf16);
    reg_new_isa("bf16", "dp2a.vv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 384LL, asimd_dp2a_vv_fp32bf16bf16);
#endif

#ifdef _ASIMD_HP_
    reg_new_isa("asimd_hp", "fmla.vs(fp16,fp16,fp16)", "FLOPS",
        0x100000LL, 384LL, asimd_fmla_vs_fp16fp16fp16);
    reg_new_isa("asimd_hp", "fmla.vv(fp16,fp16,fp16)", "FLOPS",
        0x100000LL, 384LL, asimd_fmla_vv_fp16fp16fp16);
#endif

#ifdef _ASIMD_
    reg_new_isa("asimd", "fmla.vs(f32,f32,f32)", "FLOPS",
        0x100000LL, 192LL, asimd_fmla_vs_f32f32f32);
    reg_new_isa("asimd", "fmla.vv(f32,f32,f32)", "FLOPS",
        0x100000LL, 192LL, asimd_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "fmla.vs(f64,f64,f64)", "FLOPS",
        0x100000LL, 96LL, asimd_fmla_vs_f64f64f64);
    reg_new_isa("asimd", "fmla.vv(f64,f64,f64)", "FLOPS",
        0x100000LL, 96LL, asimd_fmla_vv_f64f64f64);
#endif
#ifdef _SVE_LD1W_
    reg_new_isa("L1 Cache", "ld1w(f32)", "Byte/Cycle",
        0x186A00LL, 32LL, NULL);
    reg_new_isa("L2 Cache", "ld1w(f32)", "Byte/Cycle",
        0x186A00LL, 128LL, NULL);
    reg_new_isa("asimd", "sve_fmla.vs(f32,f32,f32)", "FLOPS",
        0x100000LL, 12LL, sve_fmla_vs_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vv(f32,f32,f32)", "FLOPS",
        0x100000LL, 12LL, sve_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vs(f64,f64,f64)", "FLOPS",
        0x100000LL, 6LL, sve_fmla_vs_f64f64f64);
    reg_new_isa("asimd", "sve_fmla.vv(f64,f64,f64)", "FLOPS",
        0x100000LL, 6LL, sve_fmla_vv_f64f64f64);
#endif

#ifdef _SME_
    reg_new_isa("SME", "sme_bfmopa.vv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme_bfmopa_vv_f32bf16bf16);
    reg_new_isa("SME", "sme_fmopa.vv(f32,f32,f32)", "FLOPS",
        0x100000LL, 24LL, sme_fmopa_vv_f32f32f32);
    reg_new_isa("SME", "sme_fmopa.vv(f32,f16,f16)", "FLOPS",
        0x100000LL, 96LL, sme_fmopa_vv_f32f16f16);
    reg_new_isa("SME", "sme_smopa.vv(i32,i8,i8)", "FLOPS",
        0x100000LL, 192LL, sme_smopa_vv_i32i8i8);
#endif

#ifdef _SME2_
    reg_new_isa("SME2", "sme2_bfmlal.vs(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 24LL, sme2_bfmlal_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.vs(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme2_bfmlal4_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal.vv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 24LL, sme2_bfmlal_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.vv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme2_bfmlal4_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal.mvv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 24LL, sme2_bfmlal_mvv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.mvv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme2_bfmlal4_mvv_f32bf16bf16);

    reg_new_isa("SME2", "sme2_bfdot.vs(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 24LL, sme2_bfdot_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.vs(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme2_bfdot4_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot.vv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 24LL, sme2_bfdot_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.vv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme2_bfdot4_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot.mvv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 24LL, sme2_bfdot_mvv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.mvv(f32,bf16,bf16)", "FLOPS",
        0x100000LL, 96LL, sme2_bfdot4_mvv_f32bf16bf16);
    
    reg_new_isa("SME2", "sme2_fmla.vs(f32,f32,f32)", "FLOPS",
        0x100000LL, 12LL, sme2_fmla_vs_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.vs(f32,f32,f32)", "FLOPS",
        0x100000LL, 48LL, sme2_fmla4_vs_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.vv(f32,f32,f32)", "FLOPS",
        0x100000LL, 12LL, sme2_fmla_vv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.vv(f32,f32,f32)", "FLOPS",
        0x100000LL, 48LL, sme2_fmla4_vv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.mvv(f32,f32,f32)", "FLOPS",
        0x100000LL, 12LL, sme2_fmla_mvv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.mvv(f32,f32,f32)", "FLOPS",
        0x100000LL, 48LL, sme2_fmla4_mvv_f32f32f32);

    reg_new_isa("SME2", "sme2_fmlal.vs(f32,f16,f16)", "FLOPS",
        0x100000LL, 24LL, sme2_fmlal_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.vs(f32,f16,f16)", "FLOPS",
        0x100000LL, 96LL, sme2_fmlal4_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal.vv(f32,f16,f16)", "FLOPS",
        0x100000LL, 24LL, sme2_fmlal_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.vv(f32,f16,f16)", "FLOPS",
        0x100000LL, 96LL, sme2_fmlal4_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal.mvv(f32,f16,f16)", "FLOPS",
        0x100000LL, 24LL, sme2_fmlal_mvv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.mvv(f32,f16,f16)", "FLOPS",
        0x100000LL, 96LL, sme2_fmlal4_mvv_f32f16f16);
#endif


    reg_new_isa("L1 Cache", "ldp(f32)", "Byte/Cycle",
        0x186A00LL, 32LL, NULL);
    reg_new_isa("L2 Cache", "ldp(f32)", "Byte/Cycle",
        0x186A00LL, 128LL, NULL);
    reg_new_isa("MULTI_ISSUE", "ldr/fmla", "IPC",
        0x186A00LL, 50LL, NULL);
}

int main(int argc, char *argv[])
{
    vector<int> set_of_threads;
    uint32_t idle_time = 0;

    bool params_enough = false;

    int i;
    for (i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--thread_pool=", 14) == 0)
        {
            parse_thread_pool(argv[i] + 14, set_of_threads);
            params_enough = true;
        }
        else if (strncmp(argv[i], "--idle_time=", 12) == 0)
        {
            idle_time = (uint32_t)atoi(argv[i] + 12);
        }
    }
    if (!params_enough)
    {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --idle_time parameter.\n");
        fprintf(stderr, "Usage: %s --thread_pool=[xxx] --idle_time=yyy\n", argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr, "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        exit(0);
    }

    cpufp_register_isa();

    cpubm_do_bench(set_of_threads, idle_time);

    return 0;
}
