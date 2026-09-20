#include "cache_bandwidth.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include "common.hpp"

namespace cpufb {

namespace {

const uint64_t kBytesPerSample = 1ULL << 30;
const int kSamples = 5;
// Several hand-unrolled SVE/SME kernels issue their last load just past the
// end of the walk; the guard keeps that inside the mapping.
const size_t kGuardBytes = 4096;

struct Job
{
    const LoadKernel *kernel;
    float *data; // worker i owns data + i * stride_bytes
    size_t stride_bytes;
    int count;
    int64_t passes;
    bool initialise; // first touch instead of reading
    std::atomic<uint64_t> cycles;
    std::atomic<int> counted_workers;
};

void worker(void *argument)
{
    Job *job = static_cast<Job *>(argument);
    // The pool position, not the arrival order, selects the buffer: the
    // worker that placed and warmed a buffer must be the one timed on it.
    const size_t index = tpool_worker_index();
    float *data = reinterpret_cast<float *>(
        reinterpret_cast<char *>(job->data) + index * job->stride_bytes);
    if (job->initialise) {
        const size_t floats = job->stride_bytes / sizeof(float);
        for (size_t i = 0; i < floats; ++i) data[i] = static_cast<float>(i);
        return;
    }
#ifdef __linux__
    PerfEventCycle counter(0, false);
    if (counter.available()) {
        counter.start();
        job->kernel->function(data, job->count, job->passes);
        counter.stop();
        if (counter.get_cycle() > 0) {
            job->cycles.fetch_add(static_cast<uint64_t>(counter.get_cycle()),
                std::memory_order_relaxed);
            job->counted_workers.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
#endif
    job->kernel->function(data, job->count, job->passes);
}

// CPUFB_DEBUG_LOAD_COVERAGE=1: run the kernel once over never-touched memory
// and count the pages that became resident.  Fewer than all of them would
// mean the kernel skips part of the working set it is credited with.
void report_coverage(const LoadKernel &kernel, int count, size_t workset_bytes)
{
    const char *enabled = std::getenv("CPUFB_DEBUG_LOAD_COVERAGE");
    if (enabled == nullptr || *enabled == '\0' ||
        std::strcmp(enabled, "0") == 0)
        return;
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) return;
    const size_t page = static_cast<size_t>(page_size);
    const size_t mapped =
        (workset_bytes + kGuardBytes + page - 1) / page * page;
    void *region = mmap(
        nullptr, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) return;
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
    (void)madvise(region, mapped, MADV_NOHUGEPAGE);
#endif
    kernel.function(static_cast<float *>(region), count, 1);

    const size_t expected = (workset_bytes + page - 1) / page;
#ifdef __APPLE__
    std::vector<char> residency(mapped / page);
#else
    std::vector<unsigned char> residency(mapped / page);
#endif
    size_t touched = 0;
    if (mincore(region, mapped, residency.data()) == 0)
        for (size_t i = 0; i < expected; ++i) touched += residency[i] & 1;
    std::fprintf(stderr,
        "load coverage: %-20s read %zu of %zu pages (%zu KiB)\n",
        kernel.name.c_str(), touched, expected, workset_bytes / 1024);
    munmap(region, mapped);
}

} // namespace

size_t cache_level_workset(size_t lower_capacity_bytes, size_t capacity_bytes)
{
    if (capacity_bytes == 0) return 0;
    if (lower_capacity_bytes == 0 || lower_capacity_bytes >= capacity_bytes)
        return capacity_bytes / 2;
    return static_cast<size_t>(
        std::sqrt(static_cast<double>(lower_capacity_bytes) * capacity_bytes));
}

CacheBandwidth measure_cache_bandwidth(const LoadKernel &kernel,
    size_t workset_bytes, tpool_t *pool, double clock_hz)
{
    CacheBandwidth result;
    if (kernel.function == nullptr || kernel.bytes_per_count == 0 ||
        kernel.block_bytes == 0 || pool == nullptr || pool->thread_num == 0)
        return result;

    // Whole loop bodies only, so that a pass reads exactly the working set.
    workset_bytes = workset_bytes / kernel.block_bytes * kernel.block_bytes;
    if (workset_bytes == 0) return result;
    const size_t workers = pool->thread_num;
    const size_t stride = (workset_bytes + kGuardBytes + 4095) / 4096 * 4096;

    void *allocation = nullptr;
    if (posix_memalign(&allocation, 4096, stride * workers) != 0) return result;

    Job job;
    job.cycles.store(0, std::memory_order_relaxed);
    job.counted_workers.store(0, std::memory_order_relaxed);
    job.kernel = &kernel;
    job.data = static_cast<float *>(allocation);
    job.stride_bytes = stride;
    job.count = static_cast<int>(workset_bytes / kernel.bytes_per_count);
    job.passes = static_cast<int64_t>(
        std::max<uint64_t>(1, kBytesPerSample / workset_bytes));
    report_coverage(kernel, job.count, workset_bytes);

    timespec start, end;
    job.initialise = true;
    tpool_run_all(pool, worker, &job, &start, &end);
    job.initialise = false;
    tpool_run_all(pool, worker, &job, &start, &end); // warm the level

    double best_seconds = 0.0;
    double best_cycles_per_core = 0.0;
    for (int sample = 0; sample < kSamples; ++sample) {
        job.cycles.store(0, std::memory_order_relaxed);
        job.counted_workers.store(0, std::memory_order_relaxed);
        if (!tpool_run_all(pool, worker, &job, &start, &end)) break;
        const double seconds = get_time(&start, &end);
        if (seconds > 0.0 && (best_seconds == 0.0 || seconds < best_seconds))
            best_seconds = seconds;
        if (job.counted_workers.load() == static_cast<int>(workers)) {
            const double cycles =
                static_cast<double>(job.cycles.load()) / workers;
            if (best_cycles_per_core == 0.0 || cycles < best_cycles_per_core)
                best_cycles_per_core = cycles;
        }
    }
    std::free(allocation);
    if (best_seconds <= 0.0) return result;

    const double bytes_per_core =
        static_cast<double>(job.passes) * workset_bytes;
    result.workset_bytes = workset_bytes;
    result.worker_count = workers;
    result.gb_per_second = bytes_per_core * workers / best_seconds * 1e-9;
    if (best_cycles_per_core > 0.0) {
        result.bytes_per_cycle = bytes_per_core / best_cycles_per_core;
        result.cycle_source = "perf_event cycles";
    } else if (clock_hz > 0.0) {
        result.bytes_per_cycle = bytes_per_core / (best_seconds * clock_hz);
        result.cycle_source = "time x clock";
    }
    if (kernel.bytes_per_load > 0)
        result.load_ipc = result.bytes_per_cycle / kernel.bytes_per_load;
    return result;
}

} // namespace cpufb
