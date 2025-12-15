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
#include<frequency.hpp>
#include<load.hpp>
#include<compute.hpp>
#include <cmath>
using namespace std;
static int64_t g_latency = 0;
static struct CacheData cache_size;

typedef struct
{
    std::string isa;
    std::string type;
    std::string dim;
    int64_t loop_time;
    int64_t comp_pl;
    void (*bench)(int64_t);
} cpubm_t;
static vector<cpubm_t> bm_list;

static double get_time(struct timespec *start,
    struct timespec *end)
{
    return end->tv_sec - start->tv_sec +
        (end->tv_nsec - start->tv_nsec) * 1e-9;
}

static void reg_new_isa(std::string isa,
    std::string type,
    std::string dim,
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

static void cpubm_riscv64_one(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{
    struct timespec start, end;
    double time_used, perf, IPC;
    char perfUnit = 'G';

    int i;
    int num_threads = tm->thread_num;

    // warm up
    for (i = 0; i < num_threads; i++)
    {
        tpool_add_work(tm, thread_func, (void*)&item);
    }
    tpool_wait(tm);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (i = 0; i < num_threads; i++)
    {
        tpool_add_work(tm, thread_func, (void*)&item);
    }
    tpool_wait(tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);

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
    IPC = item.loop_time * 24 * tm->thread_num / time_used / freq[0] / 1e9;
    stringstream ss1, ss2;
    ss1 << setprecision(5) << perf << " " << perfUnit << item.dim;
    if (item.type.find("latency") != string::npos) {
        g_latency = round(1 / IPC);
    } else {
        vector<string> cont;
        cont.resize(table.getCol());
        // cout << "table size = " << table.getCol() << endl;
        cont[0] = item.isa;
        cont[1] = item.type;
        cont[2] = ss1.str();
        cont[3] = to_string(IPC);
        cont[4] = g_latency != 0 ? to_string(g_latency) : "-";
        g_latency = 0;
        table.addOneItem(cont);
    }

}

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

    ti.resize(6);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwith";
    ti[3] = "Theory Size";
    ti[4] = "Test Size";
    ti[5] = "Latency";
    tables[1]->setColumnNum(ti.size());
    tables[1]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Theory";
    ti[2] = "Test";
    tables[2]->setColumnNum(ti.size());
    tables[2]->addOneItem(ti);

    #ifdef _SVE_
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
static void cpubm_riskv64_cache(std::vector<int> &set_of_threads,Table &table)
{
    vector<string> cont;

    
    cont.resize(table.getCol());
    cout << "cpubm_riskv64_cache" << endl;
    // get_cacheline(&cache_size, set_of_threads[0]);
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


static void cpubm_do_bench(std::vector<int> &set_of_threads,
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

        // set thread pool
        tpool_t *tm;
        tm = tpool_create(set_of_threads);

        get_cpu_freq(set_of_threads, *tables[3]);

        cpubm_riskv64_cache(set_of_threads, *tables[2]);

        // traverse task list
        
        for (i = 1; i < bm_list.size(); i++)
        {
            sleep(idle_time);
            if (bm_list[i].dim.find("OPS") != string::npos) {
                cpubm_riscv64_one(tm, bm_list[i], *tables[0]);
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


static void cpufb_register_isa()
{
#ifdef _IME_
    reg_new_isa("ime", "vmadot(s32,s8,s8)", "OPS",
        0x10000000LL, 3584LL, ime_vmadot_s32s8s8);
    reg_new_isa("ime", "vmadotu(u32,u8,u8)", "OPS",
        0x10000000LL, 3584LL, ime_vmadotu_u32u8u8);
    reg_new_isa("ime", "vmadotus(s32,u8,s8)", "OPS",
        0x10000000LL, 3584LL, ime_vmadotus_s32u8s8);
    reg_new_isa("ime", "vmadotsu(s32,s8,u8)", "OPS",
        0x10000000LL, 3584LL, ime_vmadotsu_s32s8u8);
    reg_new_isa("ime", "vmadotslide(s32,s8,s8)", "OPS",
        0x10000000LL, 3072LL, ime_vmadotslide_s32s8s8);
#endif

#ifdef _VECTOR_
    size_t avl = 0;
    __asm__ volatile("vsetvli %[avl], x0, e16, m1\n\t"
                    : [avl] "=r" (avl)
                    :
                    : "cc");
    reg_new_isa("vector", "vfmacc.vf(f16,f16,f16)", "FLOPS",
        0x10000000LL, 48LL * avl, vector_vfmacc_vf_f16f16f16);
    reg_new_isa("vector", "vfmacc.vv(f16,f16,f16)", "FLOPS",
        0x10000000LL, 48LL * avl, vector_vfmacc_vv_f16f16f16);

    __asm__ volatile("vsetvli %[avl], x0, e32, m1\n\t"
                    : [avl] "=r" (avl)
                    :
                    : "cc");
    reg_new_isa("vector", "vfmacc.vf(f32,f32,f32)", "FLOPS",
        0x10000000LL, 48LL * avl, vector_vfmacc_vf_f32f32f32);
    reg_new_isa("vector", "vfmacc.vv(f32,f32,f32)", "FLOPS",
        0x10000000LL, 48LL * avl, vector_vfmacc_vv_f32f32f32);

    __asm__ volatile("vsetvli %[avl], x0, e64, m1\n\t"
                    : [avl] "=r" (avl)
                    :
                    : "cc");
    reg_new_isa("vector", "vfmacc.vf(f64,f64,f64)", "FLOPS",
        0x10000000LL, 48LL * avl, vector_vfmacc_vf_f64f64f64);
    reg_new_isa("vector", "vfmacc.vv(f64,f64,f64)", "FLOPS",
        0x10000000LL, 48LL * avl, vector_vfmacc_vv_f64f64f64);
#endif
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

    cpufb_register_isa();
    cpubm_do_bench(set_of_threads, idle_time);

    return 0;
}

