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
typedef void (*load_bench)(float *, int, int64_t);

struct load_bench_result
{
    double seconds = 0.0;
    uint64_t cycle_sum = 0;
    size_t cycle_worker_count = 0;
};

struct load_bench_task
{
    load_bench bench;
    float *cache_data;
    int inner_loop;
    int64_t looptime;
    size_t worker_stride_bytes;
    std::atomic<size_t> next_worker;
#ifdef __linux__
    std::vector<uint64_t> worker_cycles;
#endif

    load_bench_task(load_bench bench_value, float *cache_data_value,
        int inner_loop_value, int64_t looptime_value,
        size_t worker_stride_bytes_value, size_t worker_count)
        : bench(bench_value), cache_data(cache_data_value),
          inner_loop(inner_loop_value), looptime(looptime_value),
          worker_stride_bytes(worker_stride_bytes_value), next_worker(0)
#ifdef __linux__
          ,
          worker_cycles(worker_count, 0)
#endif
    {
    }
};

static void load_bench_thread_func(void *params)
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    load_bench_task *task = reinterpret_cast<load_bench_task *>(params);
    // Index by pool position, not arrival order: the slice a worker warmed
    // into its private L1/L2 must be the slice it is later timed on.
    size_t worker_index = tpool_worker_index();
    if (worker_index == SIZE_MAX)
        worker_index =
            task->next_worker.fetch_add(1, std::memory_order_relaxed);
    float *worker_data =
        reinterpret_cast<float *>(reinterpret_cast<char *>(task->cache_data) +
            worker_index * task->worker_stride_bytes);
#ifdef __linux__
    // Count the load kernel itself instead of inferring its cycle count from
    // a separately calibrated frequency and wall-clock duration.
    PerfEventCycle cycle_counter(0, false);
    cycle_counter.start();
#endif
    task->bench(worker_data, task->inner_loop, task->looptime);
#ifdef __linux__
    cycle_counter.stop();
    const long long cycles = cycle_counter.get_cycle();
    if (cycles > 0) {
        task->worker_cycles[worker_index] = static_cast<uint64_t>(cycles);
    }
#endif
}

static load_bench_result run_load_bench(load_bench bench, float *cache_data,
    int inner_loop, int64_t looptime, size_t worker_stride_bytes, tpool_t *tm)
{
    load_bench_result result;
    struct timespec start, end;
    if (tm == nullptr || tm->thread_num == 0) {
#ifdef __linux__
        PerfEventCycle cycle_counter(0, false);
#endif
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#ifdef __linux__
        cycle_counter.start();
#endif
        bench(cache_data, inner_loop, looptime);
#ifdef __linux__
        cycle_counter.stop();
        const long long cycles = cycle_counter.get_cycle();
        if (cycles > 0) {
            result.cycle_sum = static_cast<uint64_t>(cycles);
            result.cycle_worker_count = 1;
        }
#endif
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        result.seconds = get_time(&start, &end);
        return result;
    }

    load_bench_task task(bench, cache_data, inner_loop, looptime,
        worker_stride_bytes, tm->thread_num);
    if (!tpool_run_all(tm, load_bench_thread_func, &task, &start, &end)) {
        std::cerr << "Error: failed to run load benchmark on every worker"
                  << std::endl;
        return result;
    }
    result.seconds = get_time(&start, &end);
#ifdef __linux__
    for (const uint64_t cycles : task.worker_cycles) {
        if (cycles > 0) {
            result.cycle_sum += cycles;
            ++result.cycle_worker_count;
        }
    }
#endif
    return result;
}

// Fallback normalization when PMU counters cannot be used.  For a
// multi-worker result, use the participating workers' mean clock rather than
// arbitrarily using the first worker's clock.  On macOS get_cpu_freq() may
// only populate one entry, hence the validity filter.
static double mean_measured_freq_ghz()
{
    double total_ghz = 0.0;
    size_t valid_count = 0;
    for (const double ghz : freq) {
        if (std::isfinite(ghz) && ghz > 0.0) {
            total_ghz += ghz;
            ++valid_count;
        }
    }
    return valid_count == 0 ? 0.0 : total_ghz / valid_count;
}

extern "C"
{
    void load_ptr(int looptime, int64_t *ptr);
}

static inline int get_load_bytes_per_inner_loop(const string &type)
{
#ifdef _SVE_
    if (type.find("sve-ld1") != string::npos) {
        return static_cast<int>(16 * load_sve_vector_bytes());
    }
#endif
    if (type.find("neon-ld1") != string::npos) {
        return 256;
    }
    if (type.find("ldrq-4x1") != string::npos ||
        type.find("ldr.q") != string::npos) {
        return 256;
    }
    if (type.find("ld1w") != string::npos || type.find("ZA") != string::npos) {
        return static_cast<int>(sizeof(float));
    }
    if (type.find("ldp") != string::npos) {
        return 512;
    }
    return 512;
}

static inline size_t get_load_workset_alignment(const string &type)
{
#ifdef _SME_
    // ZA slice and SME2 multi-vector kernels use x3 as a scalar-element
    // count, then consume 16 streaming vectors per loop body.  Their local
    // workset must end on that whole-body boundary.
    if (type.find("ZA") != string::npos || type == "ld1w(f32)") {
        return static_cast<size_t>(16 * load_sme_vector_bytes());
    }
#endif
    return static_cast<size_t>(get_load_bytes_per_inner_loop(type));
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
    }
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
    cache_data->test_cacheline =
        cpufb::probe_cacheline_size(cache_data->theory_cacheline,
            flush_cache_line, finish_cache_line_flush);
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

// CPUFB_DEBUG_LOAD_COVERAGE=1: prove that one outer loop of a load kernel
// really reads the whole workset it is credited with.  The kernel runs once
// over freshly mapped, never-touched memory; every page it reads becomes
// resident, and mincore() counts them.  Anything short of the full page count
// means the kernel skips data and its bandwidth is over-reported.
static void report_load_coverage(
    const string &type, load_bench bench, int inner_loop, size_t data_bytes)
{
    const char *enabled = getenv("CPUFB_DEBUG_LOAD_COVERAGE");
    if (enabled == nullptr || *enabled == '\0' || strcmp(enabled, "0") == 0)
        return;

    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return;
    const size_t page = static_cast<size_t>(page_size);
    // Same guard as the timed buffer: some kernels read a little past the end.
    const size_t mapped = (data_bytes + 4096 + page - 1) / page * page;
    void *region = mmap(
        nullptr, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) return;
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
    (void)madvise(region, mapped, MADV_NOHUGEPAGE);
#endif
    bench(static_cast<float *>(region), inner_loop, 1);

    const size_t pages = mapped / page;
    const size_t expected = (data_bytes + page - 1) / page;
#ifdef __APPLE__
    vector<char> residency(pages);
    const int status = mincore(region, mapped, residency.data());
#else
    vector<unsigned char> residency(pages);
    const int status = mincore(region, mapped, residency.data());
#endif
    size_t touched = 0;
    for (size_t i = 0; status == 0 && i < expected; ++i)
        touched += (residency[i] & 1) ? 1 : 0;
    fprintf(stderr,
        "load coverage: %-20s read %zu of %zu pages (%zu KiB workset)%s\n",
        type.c_str(), touched, expected, data_bytes / 1024,
        status != 0 ? " [mincore failed]" : "");
    munmap(region, mapped);
}

LoadBandwidth get_bandwith(
    uint64_t looptime, double data_size, string type, void *bench, tpool_t *tm)
{
    LoadBandwidth perf;
    double best_time_used = 0.0;
    uint64_t best_cycle_sum = 0;
    size_t best_cycle_worker_count = 0;
    int inner_loop;
    // Keep every instruction sample long enough for a stable clock reading, but
    // do not let a large LLC/L2 workset turn the full instruction table into a
    // multi-minute benchmark.  One outer loop walks the whole workset once.
    constexpr uint64_t kTargetBytesPerSample = 1ULL << 30; // 1 GiB
    data_size /= 2.0;
    if (data_size > 32 * 1024) {
        data_size = 32 * 1024;
    }
    const size_t requested_data_bytes = static_cast<size_t>(data_size) * 1024U;
    // Several hand-written, unrolled SME/SVE load kernels issue their last
    // vector load just past the logical end of the walk.  Keep a guard region
    // so a workset ending at a page boundary cannot fault; only data_bytes is
    // counted in the reported bandwidth.
    constexpr size_t kLoadGuardBytes = 4096;
    const size_t thread_num =
        (tm != nullptr && tm->thread_num > 0) ? tm->thread_num : 1;
    const size_t workset_alignment = get_load_workset_alignment(type);
    // Cache levels in the load table describe the working set seen by one
    // core.  Keep that per-worker working set constant during a scaling run;
    // otherwise a high-thread-count L2 sample can shrink below L1 capacity.
    // Workers still use disjoint storage, so aggregate traffic scales with
    // the worker count without sharing cache lines.
    const size_t data_bytes =
        requested_data_bytes / workset_alignment * workset_alignment;
    if (data_bytes == 0) return perf;
    const size_t aggregate_data_bytes = data_bytes * thread_num;
    const uint64_t bytes_per_outer_loop = static_cast<uint64_t>(data_bytes);
    uint64_t measured_looptime =
        max<uint64_t>(1, kTargetBytesPerSample / bytes_per_outer_loop);
    measured_looptime = min<uint64_t>(measured_looptime, looptime);
    const size_t worker_stride_bytes = data_bytes + kLoadGuardBytes;
    if (worker_stride_bytes > std::numeric_limits<size_t>::max() / thread_num)
        return perf;
    // Line-aligned storage: malloc() hands out 16-mod-64 addresses for large
    // blocks, which makes wide vector loads straddle cache lines.
    void *allocation = nullptr;
    if (posix_memalign(&allocation, 4096, worker_stride_bytes * thread_num) !=
        0)
        return perf;
    float *cache_data = static_cast<float *>(allocation);

    // Each worker owns a disjoint, equal-sized workset.  This avoids
    // synchronized reads of the same cache lines and keeps every worker's
    // stream in the selected per-core cache level.
    for (size_t worker = 0; worker < thread_num; ++worker) {
        float *worker_data =
            reinterpret_cast<float *>(reinterpret_cast<char *>(cache_data) +
                worker * worker_stride_bytes);
        for (size_t i = 0; i < data_bytes / sizeof(float); i++) {
            worker_data[i] = static_cast<float>(i + worker);
        }
    }
    inner_loop =
        static_cast<int>(data_bytes / get_load_bytes_per_inner_loop(type));
    if (inner_loop < 1) {
        inner_loop = 1;
    }

    load_bench bench_ptr = reinterpret_cast<load_bench>(bench);
    report_load_coverage(type, bench_ptr, inner_loop, data_bytes);
    // warm up
    run_load_bench(bench_ptr, cache_data, inner_loop, measured_looptime,
        worker_stride_bytes, tm);
#ifdef __APPLE__
    constexpr int repeat = 5;
#else
    constexpr int repeat = 10;
#endif
    for (int i = 0; i < repeat; i++) {
        const load_bench_result sample = run_load_bench(bench_ptr, cache_data,
            inner_loop, measured_looptime, worker_stride_bytes, tm);
        if (sample.seconds > 0.0 &&
            (best_time_used == 0.0 || sample.seconds < best_time_used)) {
            best_time_used = sample.seconds;
        }
        if (sample.cycle_worker_count == thread_num && sample.cycle_sum > 0 &&
            (best_cycle_sum == 0 || sample.cycle_sum < best_cycle_sum)) {
            best_cycle_sum = sample.cycle_sum;
            best_cycle_worker_count = sample.cycle_worker_count;
        }
    }
    // Byte/Cycle is reported per core so that it is comparable with the
    // per-core load-port limit and with the single-core x86 rows; the
    // aggregate traffic of a multi-core pool is carried by GB/s.
    const double bytes_per_worker = (double)measured_looptime * data_bytes;
    perf.workset_bytes = data_bytes;
    perf.thread_num = thread_num;
    if (best_time_used > 0.0)
        perf.gb_per_second =
            bytes_per_worker * thread_num / best_time_used * 1e-9;
    if (best_cycle_worker_count == thread_num) {
        // PMU counts taken around the actual load kernels; cycle_sum /
        // thread_num is the mean cycle count of one worker.
        perf.bytes_per_cycle = bytes_per_worker * thread_num / best_cycle_sum;
        perf.cycle_source = "perf_event cycles";
    } else {
        const double mean_freq_ghz = mean_measured_freq_ghz();
        if (best_time_used > 0.0 && mean_freq_ghz > 0.0) {
            perf.bytes_per_cycle =
                bytes_per_worker / (best_time_used * mean_freq_ghz * 1e9);
            perf.cycle_source = "time x measured frequency";
        }
    }
    free(cache_data);
    return perf;
}
