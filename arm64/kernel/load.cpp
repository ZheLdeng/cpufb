#include <algorithm>
#include <climits>
#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <vector>
#include <atomic>
#include <iostream>
#include <associativity_probe.hpp>
#include <cache_curve.hpp>
#include <cacheline_probe.hpp>
#include <cache_topology.hpp>
#include <common.hpp>
#include <load.hpp>
#include <thread_pool.hpp>
#include <sstream>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <thread>
#include <sys/sysctl.h>
#endif

#include <sys/mman.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
// Assumed line size when neither the OS nor the probe provides one.
static constexpr int kDefaultCacheLineBytes = 64;
// Sweep limit of the capacity curve.  The plateau after the last cache level
// needs three grid points, so this covers an L2 or L3 of up to 64 MiB.
static constexpr uint64_t kCacheCurveMaxBytes = 128ULL * 1024 * 1024;

using namespace std;

double cacheline = kDefaultCacheLineBytes;
extern "C"
{
    void load_ptr(int looptime, int64_t *ptr);
}

#ifdef __APPLE__
static bool read_sysctl_u64(const char *name, uint64_t &value)
{
    uint64_t sysctl_value = 0;
    size_t size = sizeof(sysctl_value);
    if (sysctlbyname(name, &sysctl_value, &size, nullptr, 0) == 0 &&
        sysctl_value > 0) {
        value = sysctl_value;
        return true;
    }
    return false;
}

static void read_darwin_cache_info(struct CacheData *cache_data)
{
    uint64_t value = 0;

    if (cache_data->theory_cacheline <= 0) {
        if (read_sysctl_u64("hw.cachelinesize", value) && value <= INT_MAX) {
            cache_data->theory_cacheline = static_cast<int>(value);
        } else {
            cache_data->theory_cacheline = 128;
        }
    }

    if (cache_data->theory_L1 <= 0) {
        if ((read_sysctl_u64("hw.perflevel0.l1dcachesize", value) ||
                read_sysctl_u64("hw.l1dcachesize", value)) &&
            value / 1024 <= INT_MAX) {
            cache_data->theory_L1 = static_cast<int>(value / 1024);
        } else {
            cache_data->theory_L1 = 64;
        }
    }

    if (cache_data->theory_L2 <= 0) {
        if ((read_sysctl_u64("hw.perflevel0.l2cachesize", value) ||
                read_sysctl_u64("hw.l2cachesize", value)) &&
            value / 1024 <= INT_MAX) {
            cache_data->theory_L2 = static_cast<int>(value / 1024);
        } else {
            cache_data->theory_L2 = 4096;
        }
    }
}
#endif

static bool read_text_file(const string &path, string &value)
{
    ifstream input(path);
    if (!input) return false;
    input >> value;
    return !value.empty();
}

static int parse_cache_size_kb(const string &text)
{
    if (text.empty()) return 0;
    char *end = nullptr;
    double value = strtod(text.c_str(), &end);
    if (end == text.c_str() || value <= 0) return 0;
    if (*end == 'M' || *end == 'm')
        value *= 1024.0;
    else if (*end == 'G' || *end == 'g')
        value *= 1024.0 * 1024.0;
    if (value > INT_MAX) return 0;
    return static_cast<int>(value);
}

void get_reported_cache_info(struct CacheData *cache_data, int cpu_id)
{
    if (cache_data == nullptr) return;
#ifdef __APPLE__
    (void)cpu_id;
    read_darwin_cache_info(cache_data);
#elif defined(__linux__)
    for (int index = 0; index < 16; ++index) {
        string base = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
            "/cache/index" + to_string(index) + "/";
        string level_text, type, size_text, line_text, ways_text;
        if (!read_text_file(base + "level", level_text)) continue;
        read_text_file(base + "type", type);
        read_text_file(base + "size", size_text);
        read_text_file(base + "coherency_line_size", line_text);
        read_text_file(base + "ways_of_associativity", ways_text);

        int level = atoi(level_text.c_str());
        int size_kb = parse_cache_size_kb(size_text);
        int line_size = atoi(line_text.c_str());
        int ways = atoi(ways_text.c_str());
        if (level == 1 && type == "Data") {
            cache_data->theory_L1 = size_kb;
            if (line_size > 0) cache_data->theory_cacheline = line_size;
            cache_data->theory_way = ways;
        } else if (level == 2 && type != "Instruction") {
            cache_data->theory_L2 = max(cache_data->theory_L2, size_kb);
        }
    }
#else
    (void)cpu_id;
#endif
    // Leave theory_cacheline at 0 when the OS exposes nothing (Android):
    // writing the 64-byte default into it would present an assumption as a
    // reported value.  Only the working line size falls back.
    cacheline = cpufb::effective_cacheline_size(cache_data->theory_cacheline,
        cache_data->test_cacheline, kDefaultCacheLineBytes);
}

cpufb::CacheCurveResult measure_cache_hierarchy(
    struct CacheData *cache_data, int cpu_id)
{
    cpufb::CacheCurveResult result;
    if (cache_data == nullptr) return result;
    get_reported_cache_info(cache_data, cpu_id);
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
        cerr << "Warning: cache curve could not bind to CPU " << cpu_id << endl;
#endif

    // The sweep range, the sample grid and the level estimate are all fixed
    // or derived from the curve itself.  Reported sizes used to seed the grid
    // and select the jump nearest to them, which made the "measured" capacity
    // agree with the OS by construction and left it empty without topology
    // data (Android).
    const uint64_t max_bytes = kCacheCurveMaxBytes;
    const int line_size = cpufb::effective_cacheline_size(
        cache_data->theory_cacheline, cache_data->test_cacheline, 64);
    result = cpufb::measure_cache_curve(load_ptr, line_size, max_bytes);
    for (const auto &level : result.levels) {
        const int size_kb = static_cast<int>(level.capacity_bytes / 1024);
        if (level.level == "L1")
            cache_data->test_L1 = size_kb;
        else if (level.level == "L2")
            cache_data->test_L2 = size_kb;
        else if (level.level == "L3")
            cache_data->test_L3 = size_kb;
    }
    cache_data->memory_latency_ns = result.memory_latency_ns;
    cache_data->hierarchy_complete = result.reached_memory;
    return result;
}

static void flush_cache_line(void *address)
{
    asm volatile("dc civac, %0\n\t" // clean and invalidate cache line
        :
        : "r"(address)
        : "memory");
}

static void finish_cache_line_flush()
{
    asm volatile("dsb ish\n\t"
                 "isb\n\t"
        :
        :
        : "memory");
}

void get_cacheline(struct CacheData *cache_data, int cpu_id)
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
    read_data(cpu_id, &cache_data->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif
#ifdef __APPLE__
    uint64_t cacheline_bytes = 0;
    size_t size = sizeof(cacheline_bytes);
    if (sysctlbyname("hw.cachelinesize", &cacheline_bytes, &size, nullptr, 0) !=
        0) {
        perror("sysctlbyname cachelinesize failed");
    } else if (cacheline_bytes <=
        static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        cache_data->theory_cacheline = static_cast<int>(cacheline_bytes);
    }
#endif
    const int fallback_cacheline =
#ifdef __APPLE__
        128;
#else
        kDefaultCacheLineBytes;
#endif
    cache_data->test_cacheline = cpufb::probe_cacheline_size(
        cache_data->theory_cacheline, flush_cache_line, finish_cache_line_flush,
        // Largest level the latency curve found (measured, not reported).
        1024 *
            static_cast<size_t>(std::max({cache_data->test_L1,
                cache_data->test_L2, cache_data->test_L3})));
    cacheline = cpufb::effective_cacheline_size(cache_data->theory_cacheline,
        cache_data->test_cacheline, fallback_cacheline);
}

void get_cache_capacities(struct CacheData *cache_size, int cpu_id)
{
    if (cache_size == nullptr) return;

    const cpufb::CacheLevelInfo l1 = cpufb::detect_data_cache_level(cpu_id, 1);
    const cpufb::CacheLevelInfo l2 = cpufb::detect_data_cache_level(cpu_id, 2);
    const std::uint64_t max_kib =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());

    if (l1.bytes >= 1024 && l1.bytes / 1024 <= max_kib) {
        cache_size->theory_L1 = static_cast<int>(l1.bytes / 1024);
        cache_size->theory_L1_source = l1.source;
    }
    if (l2.bytes >= 1024 && l2.bytes / 1024 <= max_kib) {
        cache_size->theory_L2 = static_cast<int>(l2.bytes / 1024);
        cache_size->theory_L2_source = l2.source;
    }
}

void get_multiway(struct CacheData *cache_size, int cpu_id)
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
    read_data(
        cpu_id, &cache_size->theory_way, "/cache/index0/ways_of_associativity");
#endif
    // The previous ring used a bare 4 MiB stride: every line then sits on its
    // own page and those pages alias one DTLB set, so the first transition
    // was the DTLB associativity on cores without a fully associative L1
    // DTLB.  The shared probe cancels translation cost against a control ring
    // and needs no OS-reported geometry.
    cache_size->test_way =
        cpufb::probe_l1_associativity(static_cast<int>(cacheline));
}
