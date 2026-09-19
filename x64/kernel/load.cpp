#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <immintrin.h>
#include <algorithm>
#include <random>

#include "associativity_probe.hpp"
#include "cache_curve.hpp"
#include "cacheline_probe.hpp"
#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include "load.hpp"

#include <fstream>
#include <sstream>
#ifdef __linux__
#include<sys/syscall.h>
#endif
//cacheline长度
#define CACHE_LINE 64

using namespace std;

static void flush_cache_line(void* addr) {
    _mm_clflush(addr);  // 使用 CLFLUSH
}

static void finish_cache_line_flush()
{
    _mm_mfence();
}

void get_cacheline(struct CacheData *cache_size, int cpu_id)
{
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }
    read_data(cpu_id, &cache_size->theory_cacheline, "/cache/index0/coherency_line_size");
#endif
    cache_size->test_cacheline = probe_cacheline_size(
        cache_size->theory_cacheline, flush_cache_line,
        finish_cache_line_flush);
}

void get_theory_cache(struct CacheData *cache_size, int cpu_id)
{
#ifdef __linux__
    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_L2, "/cache/index2/size");
    read_data(cpu_id, &cache_size->theory_way,
        "/cache/index0/ways_of_associativity");
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif
}

// Dependent-load ring walk; the empty asm keeps `next` live so the chain is
// neither vectorized nor dropped.
static void chase_ring(int iterations, int64_t *buffer)
{
    int64_t next = 0;
    for (int i = 0; i < iterations; ++i) next = buffer[next];
    __asm__ volatile("" : "+r"(next) : : "memory");
}

void get_cachesize(struct CacheData *cache_size, int cpu_id)
{
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }

    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_L2, "/cache/index2/size");
#endif

    // Same OS-independent latency curve as ARM64 (common/cache_curve.cpp).
    const CacheCurveResult curve = measure_cache_curve(chase_ring,
        effective_cacheline_size(cache_size->theory_cacheline,
            cache_size->test_cacheline, CACHE_LINE),
        64ULL * 1024 * 1024);
    cache_size->test_L1 = 0;
    cache_size->test_L2 = 0;
    for (const CacheLevelEstimate &level : curve.levels) {
        const int size_kb = static_cast<int>(level.capacity_bytes / 1024);
        if (level.level == "L1") cache_size->test_L1 = size_kb;
        else if (level.level == "L2") cache_size->test_L2 = size_kb;
    }
    if (getenv("CPUFB_DEBUG_CACHE_CURVE") != NULL) {
        fprintf(stderr, "cache curve (%s):\n", curve.translation_mode.c_str());
        for (const CacheLatencyPoint &point : curve.points)
            fprintf(stderr, "  %8llu KB %8.3f ns/load\n",
                static_cast<unsigned long long>(point.working_set_bytes / 1024),
                point.latency_ns);
    }
}

void get_multiway(struct CacheData *cache_size, int cpu_id)
{
    int detected_way = 0;

#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }
    read_data(cpu_id, &cache_size->theory_way, "/cache/index0/ways_of_associativity");
    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif

    // The shared probe takes no OS-reported cache geometry, so the result is
    // an independent measurement rather than a confirmation of sysfs.
    detected_way = probe_l1_associativity(effective_cacheline_size(
        cache_size->theory_cacheline, cache_size->test_cacheline, CACHE_LINE));
    cache_size->test_way = detected_way;
}

bool bind_current_thread(int cpu_id)
{
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (cpu_id >= 0 && cpu_id < CPU_SETSIZE) CPU_SET(cpu_id, &mask);
    if (cpu_id < 0 || cpu_id >= CPU_SETSIZE ||
        sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
        return false;
    }
#else
    (void)cpu_id;
#endif
    return true;
}

LoadBandwidth get_bandwith(uint64_t looptime, double data_size,
    LoadKernel kernel)
{
    struct timespec start, end;
    LoadBandwidth result;
    if (kernel == NULL) return result;

    data_size /= 2.0;
    if (data_size > 2 * 1024) {
        data_size = 2 * 1024;
    }

    // Every kernel consumes exactly 512 bytes per inner iteration.
    const uint64_t kBytesPerInnerLoop = 512;
    const int inner_loop = static_cast<int>(data_size * 1024 / kBytesPerInnerLoop);
    if (inner_loop <= 0) return result;
    const uint64_t bytes_per_loop = inner_loop * kBytesPerInnerLoop;
    const uint64_t target_bytes = 32ULL * 1024 * 1024 * 1024;
    uint64_t effective_looptime = std::max<uint64_t>(1,
        std::min<uint64_t>(looptime, target_bytes / bytes_per_loop));

    // malloc() returns 16-mod-64 addresses for large blocks, so every zmm
    // load and every other ymm load would split a cache line and roughly
    // halve the reported bandwidth.
    void *allocation = NULL;
    if (posix_memalign(&allocation, 64, bytes_per_loop) != 0) return result;
    float *cache_data = static_cast<float*>(allocation);

    //Preventing Compiler Optimization
    for (uint64_t i = 0; i < bytes_per_loop / sizeof(float); i++) {
        cache_data[i] = i;
    }

    kernel(cache_data, inner_loop, effective_looptime);
    double best_time = 0.0;
    long long best_cycles = 0;
    for (int repeat = 0; repeat < 3; ++repeat) {
#ifdef __linux__
        PerfEventCycle cycle_counter(0, false);
        cycle_counter.start();
#endif
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        kernel(cache_data, inner_loop, effective_looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
#ifdef __linux__
        cycle_counter.stop();
        const long long cycles = cycle_counter.get_cycle();
        if (cycles > 0 && (best_cycles == 0 || cycles < best_cycles))
            best_cycles = cycles;
#endif
        const double time_used = get_time(&start, &end);
        if (time_used > 0.0 && (best_time == 0.0 || time_used < best_time))
            best_time = time_used;
    }
    free(cache_data);

    const double total_bytes =
        static_cast<double>(effective_looptime) * bytes_per_loop;
    result.workset_bytes = bytes_per_loop;
    if (best_time > 0.0) result.gb_per_second = total_bytes / best_time * 1e-9;
    if (best_cycles > 0)
        result.bytes_per_cycle = total_bytes / best_cycles;
    else if (best_time > 0.0 && !freq.empty() && freq[0] > 0.0)
        result.bytes_per_cycle = total_bytes / (best_time * freq[0] * 1e9);
    return result;
}
