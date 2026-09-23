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
#include "cache_topology.hpp"
#include "cacheline_probe.hpp"
#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include "load.hpp"

#include <fstream>
#include <sstream>
#ifdef __linux__
#include <sys/syscall.h>
#endif
// Assumed line size when neither the OS nor the probe provides one.
static constexpr int kDefaultCacheLineBytes = 64;
// Sweep limit of the capacity curve.  The plateau after the last cache level
// needs three grid points, so this covers an L2 or L3 of up to 64 MiB.
static constexpr uint64_t kCacheCurveMaxBytes = 128ULL * 1024 * 1024;

using namespace std;

static void flush_cache_line(void *addr)
{
    _mm_clflush(addr); // CLFLUSH
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
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif
    cache_size->test_cacheline = cpufb::probe_cacheline_size(
        cache_size->theory_cacheline, flush_cache_line, finish_cache_line_flush,
        // Largest level the latency curve found (measured, not reported).
        1024 *
            static_cast<size_t>(std::max({cache_size->test_L1,
                cache_size->test_L2, cache_size->test_L3})));
}

void get_theory_cache(struct CacheData *cache_size, int cpu_id)
{
#ifdef __linux__
    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_L2, "/cache/index2/size");
    read_data(
        cpu_id, &cache_size->theory_way, "/cache/index0/ways_of_associativity");
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif
}

// Dependent-load ring walk; the empty asm keeps `next` live so the chain is
// neither vectorized nor dropped.
static void chase_ring(int iterations, const int32_t *buffer)
{
    int32_t next = 0;
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
    const cpufb::CacheCurveResult curve = cpufb::measure_cache_curve(chase_ring,
        cpufb::effective_cacheline_size(cache_size->theory_cacheline,
            cache_size->test_cacheline, kDefaultCacheLineBytes),
        kCacheCurveMaxBytes);
    cache_size->test_L1 = 0;
    cache_size->test_L2 = 0;
    cache_size->test_L3 = 0;
    std::string deeper_steps;
    const std::string doubt = cpufb::describe_prefetch_doubt(curve);
    cache_size->curve_trusted = doubt.empty();
    for (const cpufb::CacheLevelEstimate &level : curve.levels) {
        // Every level the curve resolved is reported; the note says when
        // its boundary was not sharp.
        const int size_kb = static_cast<int>(level.capacity_bytes / 1024);
        // The doubt applies to the whole curve, so it goes on every level.
        std::string note = cpufb::describe_transition(level);
        if (!doubt.empty()) note = note.empty() ? doubt : note + "; " + doubt;
        if (level.level == "L1") {
            cache_size->test_L1 = size_kb;
            cache_size->test_L1_note = note;
        } else if (level.level == "L2") {
            cache_size->test_L2 = size_kb;
            cache_size->test_L2_note = note;
        } else {
            // The L3 row is the deepest level the curve resolved, not its
            // third: a Kunpeng 920 resolves a step inside its 32 MiB L3 and
            // so reports four, and naming the third one L3 made the row read
            // "DISAGREES with OS, 0.12x" about a curve that had found the
            // 32 MiB exactly.  Anything between L2 and the deepest is named
            // in the note instead.
            if (cache_size->test_L3 > 0)
                deeper_steps += (deeper_steps.empty() ? "" : ", ") +
                    cpufb::format_cache_capacity(
                        static_cast<uint64_t>(cache_size->test_L3) * 1024);
            cache_size->test_L3 = size_kb;
            cache_size->test_L3_note = note;
        }
    }
    if (!deeper_steps.empty()) {
        const std::string extra =
            "the curve also stepped at " + deeper_steps + " below this level";
        cache_size->test_L3_note = cache_size->test_L3_note.empty()
            ? extra
            : cache_size->test_L3_note + "; " + extra;
    }
    cache_size->memory_latency_ns = curve.memory_latency_ns;
    cache_size->hierarchy_complete = curve.reached_memory;
    cpufb::debug_print_cache_curve(curve);
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
    read_data(
        cpu_id, &cache_size->theory_way, "/cache/index0/ways_of_associativity");
    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif

    // The shared probe takes no OS-reported cache geometry, so the result is
    // an independent measurement rather than a confirmation of sysfs.
    const int line =
        cpufb::effective_cacheline_size(cache_size->theory_cacheline,
            cache_size->test_cacheline, kDefaultCacheLineBytes);
    detected_way = cpufb::probe_l1_associativity(line);
    cache_size->test_way = detected_way;
    cache_size->test_way_bytes = cpufb::probe_l1_way_bytes(line, detected_way);
    cache_size->test_line_from_sets = cpufb::probe_l1_line_from_sets(
        detected_way, cache_size->test_way_bytes);
    cache_size->theory_l2_way = cpufb::detect_data_cache_level(cpu_id, 2).ways;
    cache_size->test_l2_way = cpufb::probe_l2_associativity(line, detected_way);
    cache_size->theory_l2_line =
        cpufb::detect_data_cache_level(cpu_id, 2).line_bytes;
    cache_size->test_l2_line =
        cpufb::probe_l2_line_from_sets(cache_size->test_l2_way);
}
