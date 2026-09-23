#include "memory_bandwidth.hpp"

#include "perf_pmu.hpp"

#include "cache_topology.hpp"
#include "table.hpp"
#include "thread_pool.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <sched.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

using cpufb::cli::CliOptions;
using cpufb::cli::save_table_sections;
using std::string;
using std::vector;

namespace {

constexpr std::int64_t kL3StreamPasses = 8;

typedef cpufb::StreamKernelSpec KernelSpec;
using cpufb::select_stream_kernel;

struct BandwidthSample
{
    double seconds;
    double gb_per_second;
    double bytes_per_cycle;
    double load_ipc;
    bool counted_cycles = false; // bytes_per_cycle from PMU cycles
};

struct BandwidthReport
{
    std::uint64_t bytes = 0;
    std::uint64_t total_bytes = 0;
    std::size_t stream_count = 1;
    std::int64_t passes_per_sample = 1;
    string kernel;
    string workset_source;
    string cycle_source;
    BandwidthSample median = {0.0, 0.0, 0.0, 0.0};
    double minimum_gb_per_second = 0.0;
    double maximum_gb_per_second = 0.0;
};

double elapsed_seconds(const timespec &start, const timespec &end)
{
    return static_cast<double>(end.tv_sec - start.tv_sec) +
        static_cast<double>(end.tv_nsec - start.tv_nsec) * 1.0e-9;
}

string format_decimal(double value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

bool pin_current_thread(int cpu)
{
#ifdef __linux__
    if (cpu < 0 || cpu >= CPU_SETSIZE) {
        std::cerr << "Error: CPU " << cpu
                  << " is outside the supported affinity mask." << std::endl;
        return false;
    }
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        std::cerr << "Error: sched_setaffinity(" << cpu
                  << ") failed: " << std::strerror(errno) << std::endl;
        return false;
    }
#else
    (void)cpu;
#endif
    return true;
}

double read_linux_cpu_frequency_hz(int cpu)
{
#ifdef __linux__
    static const char *const files[] = {"scaling_cur_freq", "cpuinfo_cur_freq",
        "scaling_max_freq", "cpuinfo_max_freq"};
    const string base =
        "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/";
    for (const char *name : files) {
        std::ifstream input((base + name).c_str());
        double frequency_khz = 0.0;
        if (input >> frequency_khz && frequency_khz > 0.0)
            return frequency_khz * 1000.0;
    }
#else
    (void)cpu;
#endif
    return 0.0;
}

// Clock used for B/cycle and load IPC when PMU cycles are unavailable, best
// first: the user-supplied CPUFB_FREQ_GHZ, the ADD dependency-chain estimate of
// the calling thread's core (no privileges needed, so it also works on macOS
// and Android), then the OS-reported frequency.  `source` names the choice.
double read_fallback_cpu_frequency_hz(int cpu, string &source)
{
    const char *override_text = std::getenv("CPUFB_FREQ_GHZ");
    if (override_text != nullptr && *override_text != '\0') {
        char *end = nullptr;
        const double ghz = std::strtod(override_text, &end);
        if (end != override_text && *end == '\0' && std::isfinite(ghz) &&
            ghz > 0.0) {
            source = "CPUFB_FREQ_GHZ";
            return ghz * 1e9;
        }
    }
    const double chain_hz = cpufb::estimate_core_clock_hz();
    if (chain_hz > 0.0) {
        source = "ADD-chain estimate";
        return chain_hz;
    }
    double frequency = read_linux_cpu_frequency_hz(cpu);
#ifdef __APPLE__
    std::uint64_t sysctl_frequency = 0;
    size_t size = sizeof(sysctl_frequency);
    if (sysctlbyname("hw.cpufrequency", &sysctl_frequency, &size, nullptr, 0) ==
        0)
        frequency = static_cast<double>(sysctl_frequency);
#endif
    source = frequency > 0.0 ? "OS-reported frequency" : "unavailable";
    return frequency;
}

class CycleCounter
{
public:
    CycleCounter() : fd_(-1)
    {
#ifdef __linux__
        perf_event_attr event;
        std::memset(&event, 0, sizeof(event));
        event.type = PERF_TYPE_HARDWARE;
        event.size = sizeof(event);
        event.config = cpufb::perf_cycles_config_for_current_cpu();
        event.disabled = 1;
        event.exclude_kernel = 1;
        event.exclude_hv = 1;
        fd_ = static_cast<int>(
            syscall(__NR_perf_event_open, &event, 0, -1, -1, 0));
#endif
    }

    ~CycleCounter()
    {
        if (fd_ >= 0) close(fd_);
    }

    bool available() const
    {
        return fd_ >= 0;
    }

    bool start()
    {
#ifdef __linux__
        return fd_ >= 0 && ioctl(fd_, PERF_EVENT_IOC_RESET, 0) == 0 &&
            ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0) == 0;
#else
        return false;
#endif
    }

    std::uint64_t stop()
    {
#ifdef __linux__
        if (fd_ < 0 || ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0) != 0) return 0;
        std::uint64_t cycles = 0;
        if (read(fd_, &cycles, sizeof(cycles)) !=
            static_cast<ssize_t>(sizeof(cycles)))
            return 0;
        return cycles;
#else
        return 0;
#endif
    }

private:
    int fd_;
};

struct ParallelStreamTask
{
    const KernelSpec &kernel;
    float *data;
    int inner_loop;
    std::int64_t passes_per_sample;
    std::size_t stream_bytes;
    std::atomic<std::size_t> next_worker;
    vector<std::uint64_t> worker_cycles;
    vector<timespec> worker_start;
    vector<timespec> worker_end;

    ParallelStreamTask(const KernelSpec &kernel_value, float *data_value,
        int inner_loop_value, std::int64_t passes_per_sample_value,
        std::size_t stream_bytes_value, std::size_t worker_count)
        : kernel(kernel_value), data(data_value), inner_loop(inner_loop_value),
          passes_per_sample(passes_per_sample_value),
          stream_bytes(stream_bytes_value), next_worker(0),
          worker_cycles(worker_count, 0), worker_start(worker_count),
          worker_end(worker_count)
    {
    }

    void reset()
    {
        next_worker.store(0, std::memory_order_relaxed);
        std::fill(worker_cycles.begin(), worker_cycles.end(), 0);
    }
};

// Slices are owned by pool position, not arrival order: the worker that first
// touches a slice (and so decides its NUMA node and its L3 domain) is the one
// that streams it in every later phase.
std::size_t parallel_worker_index(ParallelStreamTask &task)
{
    const std::size_t pool_index = tpool_worker_index();
    if (pool_index < task.worker_cycles.size()) return pool_index;
    return task.next_worker.fetch_add(1, std::memory_order_relaxed) %
        task.worker_cycles.size();
}

float *parallel_worker_data(ParallelStreamTask &task, std::size_t worker_index)
{
    return reinterpret_cast<float *>(
        reinterpret_cast<char *>(task.data) + worker_index * task.stream_bytes);
}

void parallel_stream_initialize(void *params)
{
    ParallelStreamTask *task = reinterpret_cast<ParallelStreamTask *>(params);
    const std::size_t worker_index = parallel_worker_index(*task);
    std::memset(
        parallel_worker_data(*task, worker_index), 1, task->stream_bytes);
}

void parallel_stream_warmup(void *params)
{
    ParallelStreamTask *task = reinterpret_cast<ParallelStreamTask *>(params);
    const std::size_t worker_index = parallel_worker_index(*task);
    task->kernel.function(
        parallel_worker_data(*task, worker_index), task->inner_loop, 1);
}

void parallel_stream_measure(void *params)
{
    ParallelStreamTask *task = reinterpret_cast<ParallelStreamTask *>(params);
    const std::size_t worker_index = parallel_worker_index(*task);
    CycleCounter counter;
    const bool counter_started = counter.start();
    clock_gettime(CLOCK_MONOTONIC_RAW, &task->worker_start[worker_index]);
    task->kernel.function(parallel_worker_data(*task, worker_index),
        task->inner_loop, task->passes_per_sample);
    clock_gettime(CLOCK_MONOTONIC_RAW, &task->worker_end[worker_index]);
    if (counter_started) task->worker_cycles[worker_index] = counter.stop();
}

bool timespec_before(const timespec &left, const timespec &right)
{
    return left.tv_sec < right.tv_sec ||
        (left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec);
}

double parallel_stream_elapsed_seconds(const ParallelStreamTask &task)
{
    if (task.worker_start.empty()) return 0.0;
    timespec first = task.worker_start.front();
    timespec last = task.worker_end.front();
    for (std::size_t index = 1; index < task.worker_start.size(); ++index) {
        if (timespec_before(task.worker_start[index], first))
            first = task.worker_start[index];
        if (timespec_before(last, task.worker_end[index]))
            last = task.worker_end[index];
    }
    return elapsed_seconds(first, last);
}

BandwidthSample measure_once(const KernelSpec &kernel, float *data,
    int inner_loop, std::int64_t passes_per_sample, double bytes,
    double load_instructions, CycleCounter &counter,
    double fallback_frequency_hz)
{
    timespec start;
    timespec end;
    bool counter_started = counter.start();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    kernel.function(data, inner_loop, passes_per_sample);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    std::uint64_t cycles = counter_started ? counter.stop() : 0;

    BandwidthSample sample;
    sample.seconds = elapsed_seconds(start, end);
    sample.gb_per_second = bytes / sample.seconds / 1.0e9;
    double measured_cycles = static_cast<double>(cycles);
    sample.counted_cycles = cycles > 0;
    if (measured_cycles == 0.0 && fallback_frequency_hz > 0.0)
        measured_cycles = sample.seconds * fallback_frequency_hz;
    sample.bytes_per_cycle =
        measured_cycles > 0.0 ? bytes / measured_cycles : 0.0;
    sample.load_ipc =
        measured_cycles > 0.0 ? load_instructions / measured_cycles : 0.0;
    return sample;
}

// Memory that the automatic workset may claim: half of what is currently
// available, so first-touching the buffers cannot push the host into the OOM
// killer.  Returns 0 when the platform does not expose the figure.
std::uint64_t auto_workset_budget_bytes()
{
#if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    const long pages = sysconf(_SC_AVPHYS_PAGES);
    const long page_bytes = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_bytes > 0)
        return static_cast<std::uint64_t>(pages) *
            static_cast<std::uint64_t>(page_bytes) / 2;
#endif
    return 0;
}

bool select_memory_workset(const CliOptions &options, int cpu,
    std::size_t stream_count, std::uint64_t bytes_per_block,
    std::uint64_t &requested_bytes, string &workset_source)
{
    if (options.memory_size_set) {
        if (options.memory_size_mib >
            (std::numeric_limits<std::uint64_t>::max() >> 20)) {
            std::cerr << "Error: --memory-size-mib is too large for this build."
                      << std::endl;
            return false;
        }
        requested_bytes = options.memory_size_mib << 20;
        workset_source = "CLI --memory-size-mib";
        return true;
    }

    const cpufb::LastLevelCacheInfo cache = cpufb::detect_last_level_cache(cpu);
    requested_bytes = cpufb::recommended_stream_workset_bytes(cache);
    if (cache.bytes == 0) {
        workset_source = "auto: 256 MiB fallback (cache topology unavailable)";
    } else {
        workset_source = "auto: max(256 MiB, 4 x " +
            cpufb::format_cache_capacity(cache.bytes) + ") from " +
            cache.source;
    }

    // The recommendation is per stream, so a many-core pool multiplies it
    // into tens of GiB.  Only the aggregate has to dwarf the last-level
    // cache; shrink the per-stream share to fit the memory budget, but never
    // below a floor that still spans many pages and prefetch streams.
    const std::uint64_t kMinimumAutoStreamBytes = 32ULL << 20;
    const std::uint64_t budget = auto_workset_budget_bytes();
    if (budget > 0 && stream_count > 0 &&
        requested_bytes > budget / stream_count) {
        std::uint64_t capped = budget / stream_count;
        if (capped < kMinimumAutoStreamBytes) {
            std::cerr << "Warning: only "
                      << cpufb::format_cache_capacity(budget)
                      << " is available for " << stream_count
                      << " memory streams; skipping the automatic memory "
                      << "bandwidth run. Use --memory-size-mib to force a size."
                      << std::endl;
            return false;
        }
        if (bytes_per_block > 0) capped -= capped % bytes_per_block;
        requested_bytes = capped;
        workset_source += "; capped to " +
            cpufb::format_cache_capacity(capped) +
            "/stream by available memory";
        // Also on stderr: a smaller workset changes the number, and the
        // "Workset Source" column is easy to overlook when comparing runs.
        std::cerr << "Warning: memory stream workset capped to "
                  << cpufb::format_cache_capacity(capped)
                  << " per stream by available memory; results are not "
                  << "comparable with an uncapped run." << std::endl;
    }
    return true;
}

bool select_l3_workset(int cpu, std::uint64_t bytes_per_block,
    std::uint64_t &requested_bytes, string &workset_source)
{
    const cpufb::CacheLevelInfo l3 = cpufb::detect_data_cache_level(cpu, 3);
    if (l3.bytes == 0) return false;

    const cpufb::CacheLevelInfo l2 = cpufb::detect_data_cache_level(cpu, 2);
    // The L3 stream must evict the private L2 while remaining below L3.
    // Start at 75% of L3, then move to the midpoint if a comparatively large
    // L2 would otherwise contain the whole stream.
    std::uint64_t candidate = l3.bytes - l3.bytes / 4;
    if (l2.bytes > 0 && candidate <= l2.bytes && l3.bytes > l2.bytes) {
        candidate = l2.bytes + (l3.bytes - l2.bytes) / 2;
    }
    candidate -= candidate % bytes_per_block;
    if (candidate == 0 || candidate >= l3.bytes ||
        (l2.bytes > 0 && candidate <= l2.bytes)) {
        return false;
    }

    requested_bytes = candidate;
    workset_source = "auto: L2 < workset < L3 (" +
        cpufb::format_cache_capacity(l3.bytes) + ") from " + l3.source;
    return true;
}

// A stream sized from the OS's last level can overflow what one core actually
// reaches.  A Graviton3 reports a 32 MiB L3 shared by all its cores while one
// core's latency curve leaves cache at 16 MiB, so a 24 MiB "L3" stream there
// partly measured memory.  When the curve measured a smaller last level, each
// stream is kept to three quarters of it.
bool cap_to_measured_level(std::uint64_t measured_bytes,
    std::uint64_t bytes_per_block, std::uint64_t &bytes_per_stream,
    string &workset_source)
{
    if (measured_bytes == 0) return true;
    std::uint64_t cap = measured_bytes - measured_bytes / 4;
    cap -= cap % bytes_per_block;
    if (cap == 0 || bytes_per_stream <= cap) return true;
    bytes_per_stream = cap;
    workset_source += "; capped to " + cpufb::format_cache_capacity(cap) +
        ", 3/4 of the " + cpufb::format_cache_capacity(measured_bytes) +
        " last level one core measured";
    return true;
}

bool select_parallel_l3_workset(const vector<int> &cpus,
    std::uint64_t bytes_per_block, std::uint64_t measured_last_level_bytes,
    std::uint64_t &bytes_per_stream, string &workset_source)
{
    if (cpus.empty()) return false;

    std::uint64_t aggregate_bytes = 0;
    if (!select_l3_workset(
            cpus.front(), bytes_per_block, aggregate_bytes, workset_source)) {
        return false;
    }
    if (cpus.size() == 1) {
        bytes_per_stream = aggregate_bytes;
        cap_to_measured_level(measured_last_level_bytes, bytes_per_block,
            bytes_per_stream, workset_source);
        // Still has to leave the private L2 to be an L3 measurement.
        return bytes_per_stream >
            cpufb::detect_data_cache_level(cpus.front(), 2).bytes;
    }

    std::uint64_t largest_l2 = 0;
    for (const int cpu : cpus) {
        largest_l2 =
            std::max(largest_l2, cpufb::detect_data_cache_level(cpu, 2).bytes);
    }

    bytes_per_stream =
        aggregate_bytes / static_cast<std::uint64_t>(cpus.size());
    bytes_per_stream -= bytes_per_stream % bytes_per_block;
    // A stream no larger than its private L2 would not measure shared-L3
    // throughput.  Do not silently turn the L3 row into an L2 measurement.
    if (bytes_per_stream == 0 || bytes_per_stream <= largest_l2) return false;

    string cap_note;
    cap_to_measured_level(
        measured_last_level_bytes, bytes_per_block, bytes_per_stream, cap_note);
    if (bytes_per_stream <= largest_l2) return false;
    const std::uint64_t actual_aggregate = bytes_per_stream * cpus.size();
    const cpufb::CacheLevelInfo l3 =
        cpufb::detect_data_cache_level(cpus.front(), 3);
    workset_source = "auto: " + cpufb::format_cache_capacity(actual_aggregate) +
        " aggregate / " + cpufb::format_cache_capacity(bytes_per_stream) +
        " per stream; L2 < "
        "per-stream workset < L3 (" +
        cpufb::format_cache_capacity(l3.bytes) + ") from " + l3.source +
        cap_note;
    return true;
}

bool measure_stream_bandwidth(int cpu, const KernelSpec &kernel,
    std::uint64_t requested_bytes, const string &workset_source,
    std::uint32_t repetitions, std::uint32_t idle_time,
    std::int64_t passes_per_sample, BandwidthReport &report)
{
    if (!pin_current_thread(cpu)) return false;
    if (requested_bytes == 0 ||
        requested_bytes > static_cast<std::uint64_t>(
                              std::numeric_limits<std::size_t>::max())) {
        std::cerr
            << "Error: selected stream workset is too large for this build."
            << std::endl;
        return false;
    }

    const std::size_t bytes = static_cast<std::size_t>(requested_bytes);
    if (bytes < kernel.bytes_per_block || bytes % kernel.bytes_per_block != 0) {
        std::cerr << "Error: stream workset must be a multiple of "
                  << kernel.bytes_per_block << " bytes for " << kernel.name
                  << "." << std::endl;
        return false;
    }
    const std::uint64_t inner_loop_u64 = bytes / kernel.bytes_per_block;
    if (inner_loop_u64 > static_cast<std::uint64_t>(INT_MAX)) {
        std::cerr << "Error: stream workset is too large for the stream kernel."
                  << std::endl;
        return false;
    }
    const int inner_loop = static_cast<int>(inner_loop_u64);

    void *allocation = nullptr;
    const int allocation_status = posix_memalign(&allocation, 4096, bytes);
    if (allocation_status != 0) {
        std::cerr << "Error: failed to allocate "
                  << cpufb::format_cache_capacity(requested_bytes) << ": "
                  << std::strerror(allocation_status) << std::endl;
        return false;
    }
#ifdef __linux__
    (void)madvise(allocation, bytes, MADV_HUGEPAGE);
#endif
    std::memset(allocation, 1, bytes);
    float *data = static_cast<float *>(allocation);

    if (idle_time > 0) sleep(idle_time);
    kernel.function(data, inner_loop, 1);

    CycleCounter counter;
    string cycle_source = "perf CPU cycles";
    string fallback_source;
    // Always known: an event that opens but counts nothing (see perf_pmu.hpp)
    // is only detected after the first sample.
    const double fallback_frequency_hz =
        read_fallback_cpu_frequency_hz(cpu, fallback_source);
    if (!counter.available()) {
        cycle_source = fallback_source;
        std::cerr << "Warning: hardware CPU cycles are unavailable; ";
        if (fallback_frequency_hz > 0.0)
            std::cerr << "B/cycle and load IPC use elapsed time x "
                      << fallback_source << ".";
        else
            std::cerr << "B/cycle and load IPC are omitted.";
        std::cerr << std::endl;
    }

    const double transferred_bytes =
        static_cast<double>(bytes) * passes_per_sample;
    const double load_instructions =
        static_cast<double>(inner_loop_u64 * kernel.loads_per_block) *
        passes_per_sample;
    vector<BandwidthSample> samples;
    samples.reserve(repetitions);
    bool all_samples_have_cycles = true;
    for (std::uint32_t repetition = 0; repetition < repetitions; ++repetition) {
        BandwidthSample sample = measure_once(kernel, data, inner_loop,
            passes_per_sample, transferred_bytes, load_instructions, counter,
            fallback_frequency_hz);
        all_samples_have_cycles &= sample.counted_cycles;
        if (sample.seconds <= 0.0 || !std::isfinite(sample.gb_per_second)) {
            std::free(allocation);
            std::cerr << "Error: stream bandwidth timing failed." << std::endl;
            return false;
        }
        samples.push_back(sample);
    }
    std::free(allocation);

    std::sort(samples.begin(), samples.end(),
        [](const BandwidthSample &left, const BandwidthSample &right) {
            return left.gb_per_second < right.gb_per_second;
        });
    report.bytes = requested_bytes;
    report.total_bytes = requested_bytes;
    report.stream_count = 1;
    report.passes_per_sample = passes_per_sample;
    report.kernel = kernel.name;
    report.workset_source = workset_source;
    // The event can open and still count nothing (a PMU of another core
    // type, or a VM without a virtual PMU); the samples decide the label.
    if (!all_samples_have_cycles && cycle_source == "perf CPU cycles")
        cycle_source = fallback_frequency_hz > 0.0
            ? fallback_source + " (perf event counted 0 cycles)"
            : "unavailable (perf event counted 0 cycles)";
    report.cycle_source = cycle_source;
    report.median = samples[samples.size() / 2];
    report.minimum_gb_per_second = samples.front().gb_per_second;
    report.maximum_gb_per_second = samples.back().gb_per_second;
    return true;
}

bool measure_parallel_stream_bandwidth(const vector<int> &cpus,
    const KernelSpec &kernel, std::uint64_t requested_bytes,
    const string &workset_source, std::uint32_t repetitions,
    std::uint32_t idle_time, std::int64_t passes_per_sample,
    BandwidthReport &report)
{
    if (cpus.empty()) {
        std::cerr << "Error: no CPUs were selected for the stream benchmark."
                  << std::endl;
        return false;
    }
    if (cpus.size() == 1) {
        return measure_stream_bandwidth(cpus.front(), kernel, requested_bytes,
            workset_source, repetitions, idle_time, passes_per_sample, report);
    }
    if (requested_bytes == 0 ||
        requested_bytes > static_cast<std::uint64_t>(
                              std::numeric_limits<std::size_t>::max()) ||
        requested_bytes < kernel.bytes_per_block ||
        requested_bytes % kernel.bytes_per_block != 0) {
        std::cerr << "Error: stream workset must be a nonzero multiple of "
                  << kernel.bytes_per_block << " bytes for " << kernel.name
                  << "." << std::endl;
        return false;
    }
    if (requested_bytes > std::numeric_limits<std::uint64_t>::max() /
            static_cast<std::uint64_t>(cpus.size())) {
        std::cerr << "Error: aggregate stream workset is too large."
                  << std::endl;
        return false;
    }

    const std::uint64_t total_bytes_u64 = requested_bytes * cpus.size();
    if (total_bytes_u64 >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        std::cerr << "Error: aggregate stream workset is too large for this "
                  << "build." << std::endl;
        return false;
    }
    const std::size_t stream_bytes = static_cast<std::size_t>(requested_bytes);
    const std::size_t total_bytes = static_cast<std::size_t>(total_bytes_u64);
    const std::uint64_t inner_loop_u64 =
        requested_bytes / kernel.bytes_per_block;
    if (inner_loop_u64 > static_cast<std::uint64_t>(INT_MAX)) {
        std::cerr << "Error: stream workset is too large for the stream kernel."
                  << std::endl;
        return false;
    }

    void *allocation = nullptr;
    const int allocation_status =
        posix_memalign(&allocation, 4096, total_bytes);
    if (allocation_status != 0) {
        std::cerr << "Error: failed to allocate "
                  << cpufb::format_cache_capacity(total_bytes_u64) << ": "
                  << std::strerror(allocation_status) << std::endl;
        return false;
    }
#ifdef __linux__
    (void)madvise(allocation, total_bytes, MADV_HUGEPAGE);
#endif

    tpool_t *thread_pool = tpool_create(cpus);
    if (thread_pool == nullptr) {
        std::free(allocation);
        std::cerr << "Error: failed to create stream benchmark workers."
                  << std::endl;
        return false;
    }

    ParallelStreamTask task(kernel, static_cast<float *>(allocation),
        static_cast<int>(inner_loop_u64), passes_per_sample, stream_bytes,
        cpus.size());
    timespec start;
    timespec end;
    task.reset();
    if (!tpool_run_all(
            thread_pool, parallel_stream_initialize, &task, &start, &end)) {
        tpool_destroy(thread_pool);
        std::free(allocation);
        std::cerr << "Error: failed to initialize stream workers." << std::endl;
        return false;
    }

    if (idle_time > 0) sleep(idle_time);
    task.reset();
    if (!tpool_run_all(
            thread_pool, parallel_stream_warmup, &task, &start, &end)) {
        tpool_destroy(thread_pool);
        std::free(allocation);
        std::cerr << "Error: failed to warm stream workers." << std::endl;
        return false;
    }

    double fallback_frequency_hz = 0.0;
    bool fallback_frequency_available = true;
    string fallback_source;
    for (const int cpu : cpus) {
        // The ADD-chain estimate is taken on the calling thread, so it
        // stands for every core of a homogeneous pool.
        const double frequency_hz =
            read_fallback_cpu_frequency_hz(cpu, fallback_source);
        if (frequency_hz <= 0.0) {
            fallback_frequency_available = false;
            break;
        }
        fallback_frequency_hz += frequency_hz;
    }

    vector<BandwidthSample> samples;
    samples.reserve(repetitions);
    bool all_samples_have_cycles = true;
    for (std::uint32_t repetition = 0; repetition < repetitions; ++repetition) {
        task.reset();
        if (!tpool_run_all(
                thread_pool, parallel_stream_measure, &task, &start, &end)) {
            tpool_destroy(thread_pool);
            std::free(allocation);
            std::cerr << "Error: synchronized stream timing failed."
                      << std::endl;
            return false;
        }

        BandwidthSample sample;
        sample.seconds = parallel_stream_elapsed_seconds(task);
        sample.gb_per_second = static_cast<double>(total_bytes_u64) *
            passes_per_sample / sample.seconds / 1.0e9;
        std::uint64_t total_cycles = 0;
        bool all_workers_have_cycles = true;
        for (const std::uint64_t cycles : task.worker_cycles) {
            if (cycles == 0) {
                all_workers_have_cycles = false;
                break;
            }
            total_cycles += cycles;
        }
        all_samples_have_cycles &= all_workers_have_cycles;
        double measured_cycles =
            all_workers_have_cycles ? static_cast<double>(total_cycles) : 0.0;
        if (measured_cycles == 0.0 && fallback_frequency_available)
            measured_cycles = sample.seconds * fallback_frequency_hz;
        sample.bytes_per_cycle = measured_cycles > 0.0
            ? static_cast<double>(total_bytes_u64) * passes_per_sample /
                measured_cycles
            : 0.0;
        const double load_instructions = static_cast<double>(inner_loop_u64) *
            kernel.loads_per_block * cpus.size() * passes_per_sample;
        sample.load_ipc =
            measured_cycles > 0.0 ? load_instructions / measured_cycles : 0.0;
        if (sample.seconds <= 0.0 || !std::isfinite(sample.gb_per_second)) {
            tpool_destroy(thread_pool);
            std::free(allocation);
            std::cerr << "Error: stream bandwidth timing failed." << std::endl;
            return false;
        }
        samples.push_back(sample);
    }
    tpool_destroy(thread_pool);
    std::free(allocation);

    std::sort(samples.begin(), samples.end(),
        [](const BandwidthSample &left, const BandwidthSample &right) {
            return left.gb_per_second < right.gb_per_second;
        });
    report.bytes = requested_bytes;
    report.total_bytes = total_bytes_u64;
    report.stream_count = cpus.size();
    report.passes_per_sample = passes_per_sample;
    report.kernel = kernel.name;
    report.workset_source = workset_source;
    report.cycle_source = all_samples_have_cycles
        ? "perf CPU cycles (sum over cores)"
        : (fallback_frequency_available ? fallback_source + " (sum over cores)"
                                        : "unavailable");
    report.median = samples[samples.size() / 2];
    report.minimum_gb_per_second = samples.front().gb_per_second;
    report.maximum_gb_per_second = samples.back().gb_per_second;
    return true;
}

string format_cache_measurement_details(const BandwidthReport &report)
{
    string details = "min/max " +
        format_decimal(report.minimum_gb_per_second, 3) + "/" +
        format_decimal(report.maximum_gb_per_second, 3) + " GB/s";
    if (report.stream_count > 1) {
        details += ", aggregate of " + std::to_string(report.stream_count) +
            " synchronized streams";
    }
    if (report.passes_per_sample > 1) {
        details +=
            ", " + std::to_string(report.passes_per_sample) + " passes/sample";
    }
    if (report.median.bytes_per_cycle > 0.0) {
        details += ", " + format_decimal(report.median.bytes_per_cycle, 3) +
            " B/cycle";
    }
    if (report.median.load_ipc > 0.0) {
        details += ", IPC " + format_decimal(report.median.load_ipc, 3);
    }
    return details + "; " + report.cycle_source + "; " + report.workset_source;
}

string format_stream_workset(const BandwidthReport &report)
{
    const string per_stream = cpufb::format_cache_capacity(report.bytes);
    if (report.stream_count == 1) return per_stream;
    return per_stream + "/stream (" +
        cpufb::format_cache_capacity(report.total_bytes) + " total)";
}

void append_cache_bandwidth_row(Table &table, const string &item,
    const vector<int> &cpus, const BandwidthReport &report)
{
    vector<string> row(table.getCol());
    row[0] = item;
    row[1] = report.stream_count == 1
        ? "core " + std::to_string(cpus.front())
        : std::to_string(report.stream_count) + " cores";
    row[2] = report.kernel;
    row[3] = format_decimal(report.median.gb_per_second, 3) + " GB/s";
    row[4] = format_stream_workset(report);
    row[5] = format_cache_measurement_details(report);
    table.addOneItem(row);
}

void append_cache_bandwidth_failure_row(Table &table, const string &item)
{
    vector<string> row(table.getCol());
    row[0] = item;
    for (std::size_t i = 1; i < row.size(); ++i) row[i] = "-";
    row[row.size() - 1] = "not measured (see stderr)";
    table.addOneItem(row);
}

} // namespace

bool append_cache_memory_bandwidth(const CliOptions &options, Table &table,
    std::uint64_t measured_last_level_bytes)
{
    const vector<int> &cpus = options.thread_pool;
    const int cpu = cpus.front();
    const KernelSpec kernel = select_stream_kernel();
    std::uint64_t requested_bytes = 0;
    string workset_source;
    BandwidthReport report;

    if (select_parallel_l3_workset(cpus, kernel.bytes_per_block,
            measured_last_level_bytes, requested_bytes, workset_source)) {
        if (!measure_parallel_stream_bandwidth(cpus, kernel, requested_bytes,
                workset_source, options.memory_repetitions, options.idle_time,
                kL3StreamPasses, report)) {
            // A failed stream (typically an allocation failure) must not
            // discard the probes already collected or the remaining
            // compute/load categories; record it and carry on.
            append_cache_bandwidth_failure_row(
                table, "L3 sequential read bandwidth");
        } else {
            append_cache_bandwidth_row(table,
                cpus.size() == 1 ? "L3 sequential read bandwidth"
                                 : "L3 aggregate sequential read bandwidth",
                cpus, report);
        }
    }

    if (!select_memory_workset(options, cpu, cpus.size(),
            kernel.bytes_per_block, requested_bytes, workset_source) ||
        !measure_parallel_stream_bandwidth(cpus, kernel, requested_bytes,
            workset_source, options.memory_repetitions, options.idle_time, 1,
            report)) {
        append_cache_bandwidth_failure_row(
            table, "Memory sequential read bandwidth");
        return true;
    }
    append_cache_bandwidth_row(table,
        cpus.size() == 1 ? "Memory sequential read bandwidth"
                         : "Memory aggregate sequential read bandwidth",
        cpus, report);
    return true;
}

bool run_memory_bandwidth(const CliOptions &options)
{
    const vector<int> &cpus = options.thread_pool;
    const int cpu = cpus.front();
    const KernelSpec kernel = select_stream_kernel();
    std::uint64_t requested_bytes = 0;
    string workset_source;
    BandwidthReport report;
    if (!select_memory_workset(options, cpu, cpus.size(),
            kernel.bytes_per_block, requested_bytes, workset_source) ||
        !measure_parallel_stream_bandwidth(cpus, kernel, requested_bytes,
            workset_source, options.memory_repetitions, options.idle_time, 1,
            report)) {
        return false;
    }

    Table table;
    vector<string> row(10);
    row[0] = "Core ID";
    row[1] = "Workset";
    row[2] = "Kernel";
    row[3] = "Median GB/s";
    row[4] = "B/cycle";
    row[5] = "Load IPC";
    row[6] = "Min GB/s";
    row[7] = "Max GB/s";
    row[8] = "Cycle Source";
    row[9] = "Workset Source";
    table.setColumnNum(row.size());
    table.addOneItem(row);

    row[0] = report.stream_count == 1
        ? std::to_string(cpu)
        : std::to_string(report.stream_count) + " streams";
    row[1] = format_stream_workset(report);
    row[2] = report.kernel;
    row[3] = format_decimal(report.median.gb_per_second, 3);
    row[4] = report.median.bytes_per_cycle > 0.0
        ? format_decimal(report.median.bytes_per_cycle, 3)
        : "-";
    row[5] = report.median.load_ipc > 0.0
        ? format_decimal(report.median.load_ipc, 3)
        : "-";
    row[6] = format_decimal(report.minimum_gb_per_second, 3);
    row[7] = format_decimal(report.maximum_gb_per_second, 3);
    row[8] = report.cycle_source;
    row[9] = report.workset_source;
    table.addOneItem(row);

    if (report.stream_count == 1) {
        std::cout << "Single-core memory bandwidth: one sequential read "
                  << "stream, " << options.memory_repetitions << " sample(s)"
                  << std::endl;
    } else {
        std::cout << "Multi-core memory bandwidth: " << report.stream_count
                  << " synchronized read streams, "
                  << options.memory_repetitions << " sample(s)" << std::endl;
    }
    table.print();

    vector<std::pair<string, const Table *>> sections;
    sections.push_back(std::make_pair(string("memory_bandwidth"), &table));
    return save_table_sections(options.save, sections);
}
