#include "table.hpp"
#include "thread_pool.hpp"

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
#include <linux/perf_event.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include<load.hpp>
#include<compute.hpp>
#include<frequency.hpp>

#ifdef __linux__
#include<perf_event.hpp>
#endif
using namespace std;
extern uint64_t freq;

typedef struct
{
    std::string isa;
    std::string type;
    std::string dim;
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


static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t*)params;
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static void cpubm_arm64_one(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{
    struct timespec start, end;
    double time_used, perf;
    char perfUnit = 'G';

    int i;
    int num_threads = tm->thread_num;

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

    stringstream ss;
    ss << std::setprecision(5) << perf << " " << perfUnit << item.dim;

    vector<string> cont;
    cont.resize(3);
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
}

static void cpubm_arm_load(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{
    struct timespec start, end;
    double time_used, perf;
    cache_bm_t bm;
    int num_threads = tm->thread_num;

    float* cache_data = (float*)malloc(item.comp_pl * 1024);
    //Preventing Compiler Optimization
    for(int i = 0;i < item.comp_pl * 1024/sizeof(float); i++){
        cache_data[i]=i;
    }
    int inner_loop = item.comp_pl * 1024 / sizeof(float) / (4 * 32);
    bm.bench = load_ldp_kernel;
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

    perf = (double)item.loop_time * item.comp_pl * 1024 /
        (time_used * freq * 1e3);
   
    stringstream ss;
    
    ss << std::setprecision(5) << perf << " " << item.dim;

    vector<string> cont;
    cont.resize(3);
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
    free(cache_data);
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
        vector<string> ti;
        ti.resize(3);
        ti[0] = "Instruction Set";
        ti[1] = "Core Computation";
        ti[2] = "Peak Performance";

        Table table;
        table.setColumnNum(3);
        table.addOneItem(ti);

        // set thread pool
        tpool_t *tm;
        tm = tpool_create(set_of_threads);

        // traverse task list
        cpubm_arm64_one(tm, bm_list[0], table);
        for (i = 1; i < bm_list.size(); i++)
        {
            sleep(idle_time);
            if(bm_list[i].dim.find("OPS") != std::string::npos) {
                cpubm_arm64_one(tm, bm_list[i], table);
            } else if (bm_list[i].dim.find("Byte/Cycle") != std::string::npos) {
                cpubm_arm_load(tm, bm_list[i], table);
            } else {
                std::cout << "Wrong dimension !" << endl;
                break;
            }
        }

        table.print();

        tpool_destroy(tm);
    }
    else
    {
        printf("Sorry, there's no any supported SIMD isa.\n");
    }
}

static void parse_thread_pool(char *sets,
    vector<int> &set_of_threads)
{
    if (sets[0] != '[')
    {
        return;
    }
    int pos = 1;
    int left = 0, right = 0;
    int state = 0;
    while (sets[pos] != ']' && sets[pos] != '\0')
    {
        if (state == 0)
        {
            if (sets[pos] >= '0' && sets[pos] <= '9')
            {
                left *= 10;
                left += (int)(sets[pos] - '0');
            }
            else if (sets[pos] == ',')
            {
                set_of_threads.push_back(left);
                left = 0;
            }
            else if (sets[pos] == '-')
            {
                right = 0;
                state = 1;
            }
        }
        else if (state == 1)
        {
            if (sets[pos] >= '0' && sets[pos] <= '9')
            {
                right *= 10;
                right += (int)(sets[pos] - '0');
            }
            else if (sets[pos] == ',')
            {
                int i;
                for (i = left; i <= right; i++)
                {
                    set_of_threads.push_back(i);
                }
                left = 0;
                state = 0;
            }
        }
        pos++;
    }
    if (sets[pos] != ']')
    {
        return;
    }
    if (state == 0)
    {
        set_of_threads.push_back(left);
    }
    else if (state == 1)
    {
        int i;
        for (i = left; i <= right; i++)
        {
            set_of_threads.push_back(i);
        }
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

    reg_new_isa("LOAD_L1", "ldp(f32)", "Byte/Cycle",
        0x186A00LL, 32LL, NULL);
    reg_new_isa("LOAD_L2", "ldp(f32)", "Byte/Cycle",
        0x186A00LL, 128LL, NULL);
    // reg_new_isa("Multiway", "L1cache", "Way",
    //     0x2710LL, 512LL,cpufp_kernel_multiway);
    // reg_new_isa("Multiway", "L2cache", "Way",
    //     0x2710LL, 512LL, cpufp_kernel_multiway);
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
    get_cpu_freq(set_of_threads);
    cpubm_do_bench(set_of_threads, idle_time);

    return 0;
}
