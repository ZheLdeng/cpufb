#include "memory_bandwidth.hpp"

#include "cache_topology.hpp"
#include "load.hpp"
#include "table.hpp"

#include <algorithm>
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

#if defined(__linux__) && !defined(__APPLE__)
#include "../runtime_features.hpp"
#endif

using cpufb_cli::CliOptions;
using cpufb_cli::save_table_sections;
using std::string;
using std::vector;

namespace {

typedef void (*StreamKernel)(float *, int, int64_t);

struct KernelSpec
{
    StreamKernel function;
    string name;
    std::uint64_t bytes_per_block;
    std::uint64_t loads_per_block;
};

struct BandwidthSample
{
    double seconds;
    double gb_per_second;
    double bytes_per_cycle;
    double load_ipc;
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
    static const char *const files[] = {
        "scaling_cur_freq",
        "cpuinfo_cur_freq",
        "scaling_max_freq",
        "cpuinfo_max_freq"
    };
    const string base = "/sys/devices/system/cpu/cpu" +
        std::to_string(cpu) + "/cpufreq/";
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

double read_fallback_cpu_frequency_hz(int cpu)
{
    double frequency = read_linux_cpu_frequency_hz(cpu);
#ifdef __APPLE__
    std::uint64_t sysctl_frequency = 0;
    size_t size = sizeof(sysctl_frequency);
    if (sysctlbyname("hw.cpufrequency",
            &sysctl_frequency,
            &size,
            NULL,
            0) == 0)
        frequency = static_cast<double>(sysctl_frequency);
#endif
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
        event.config = PERF_COUNT_HW_CPU_CYCLES;
        event.disabled = 1;
        event.exclude_kernel = 1;
        event.exclude_hv = 1;
        fd_ = static_cast<int>(syscall(__NR_perf_event_open,
            &event,
            0,
            -1,
            -1,
            0));
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
        return fd_ >= 0 &&
            ioctl(fd_, PERF_EVENT_IOC_RESET, 0) == 0 &&
            ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0) == 0;
#else
        return false;
#endif
    }

    std::uint64_t stop()
    {
#ifdef __linux__
        if (fd_ < 0 || ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0) != 0)
            return 0;
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

KernelSpec select_stream_kernel()
{
#if defined(_SVE_) && defined(__linux__) && !defined(__APPLE__)
    if (arm64_runtime_features().sve) {
        const std::uint64_t vector_bytes = load_sve_vector_bytes();
        KernelSpec spec = {
            load_sve_ld1h_kernel,
            "sve-ld1h(f16) " + std::to_string(vector_bytes * 8) + "-bit",
            16 * vector_bytes,
            16
        };
        return spec;
    }
#endif
    KernelSpec spec = {
        load_neon_ld1h_4x1_kernel,
        "neon-ld1h-4x1(f16)",
        256,
        16
    };
    return spec;
}

BandwidthSample measure_once(const KernelSpec &kernel,
    float *data,
    int inner_loop,
    double bytes,
    double load_instructions,
    CycleCounter &counter,
    double fallback_frequency_hz)
{
    timespec start;
    timespec end;
    bool counter_started = counter.start();
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    kernel.function(data, inner_loop, 1);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    std::uint64_t cycles = counter_started ? counter.stop() : 0;

    BandwidthSample sample;
    sample.seconds = elapsed_seconds(start, end);
    sample.gb_per_second = bytes / sample.seconds / 1.0e9;
    double measured_cycles = static_cast<double>(cycles);
    if (measured_cycles == 0.0 && fallback_frequency_hz > 0.0)
        measured_cycles = sample.seconds * fallback_frequency_hz;
    sample.bytes_per_cycle = measured_cycles > 0.0
        ? bytes / measured_cycles
        : 0.0;
    sample.load_ipc = measured_cycles > 0.0
        ? load_instructions / measured_cycles
        : 0.0;
    return sample;
}

} // namespace

bool run_arm64_memory_bandwidth(const CliOptions &options)
{
    const int cpu = options.thread_pool[0];
    if (!pin_current_thread(cpu)) return false;

    const KernelSpec kernel = select_stream_kernel();
    std::uint64_t requested_bytes = 0;
    string workset_source;
    if (options.memory_size_set) {
        if (options.memory_size_mib >
            (std::numeric_limits<std::uint64_t>::max() >> 20)) {
            std::cerr << "Error: --memory-size-mib is too large for this build."
                      << std::endl;
            return false;
        }
        requested_bytes = options.memory_size_mib << 20;
        workset_source = "CLI --memory-size-mib";
    } else {
        const cpufb::LastLevelCacheInfo cache =
            cpufb::detect_last_level_cache(cpu);
        requested_bytes = cpufb::recommended_stream_workset_bytes(cache);
        if (cache.bytes == 0) {
            workset_source = "auto: 256 MiB fallback (cache topology unavailable)";
        } else {
            workset_source = "auto: max(256 MiB, 4 x " +
                cpufb::format_cache_capacity(cache.bytes) + ") from " +
                cache.source;
        }
    }

    if (requested_bytes == 0 ||
        requested_bytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        std::cerr << "Error: selected memory workset is too large for this build."
                  << std::endl;
        return false;
    }
    const std::size_t bytes = static_cast<std::size_t>(requested_bytes);
    if (bytes < kernel.bytes_per_block ||
        bytes % kernel.bytes_per_block != 0) {
        std::cerr << "Error: memory workset must be a multiple of "
                  << kernel.bytes_per_block << " bytes for " << kernel.name
                  << "." << std::endl;
        return false;
    }
    const std::uint64_t inner_loop_u64 = bytes / kernel.bytes_per_block;
    if (inner_loop_u64 > static_cast<std::uint64_t>(INT_MAX)) {
        std::cerr << "Error: memory workset is too large for the stream kernel."
                  << std::endl;
        return false;
    }
    const int inner_loop = static_cast<int>(inner_loop_u64);

    void *allocation = NULL;
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

    if (options.idle_time > 0) sleep(options.idle_time);
    kernel.function(data, inner_loop, 1);

    CycleCounter counter;
    const double fallback_frequency_hz =
        counter.available() ? 0.0 : read_fallback_cpu_frequency_hz(cpu);
    string cycle_source = "perf CPU cycles";
    if (!counter.available()) {
        cycle_source = fallback_frequency_hz > 0.0
            ? "cpufreq estimate"
            : "unavailable";
        std::cerr << "Warning: hardware CPU cycles are unavailable; ";
        if (fallback_frequency_hz > 0.0)
            std::cerr << "B/cycle and load IPC use a cpufreq estimate.";
        else
            std::cerr << "B/cycle and load IPC are omitted.";
        std::cerr << std::endl;
    }

    const double transferred_bytes = static_cast<double>(bytes);
    const double load_instructions =
        static_cast<double>(inner_loop_u64 * kernel.loads_per_block);
    vector<BandwidthSample> samples;
    samples.reserve(options.memory_repetitions);
    for (std::uint32_t repetition = 0;
         repetition < options.memory_repetitions;
         ++repetition) {
        BandwidthSample sample = measure_once(kernel,
            data,
            inner_loop,
            transferred_bytes,
            load_instructions,
            counter,
            fallback_frequency_hz);
        if (sample.seconds <= 0.0 || !std::isfinite(sample.gb_per_second)) {
            std::free(allocation);
            std::cerr << "Error: memory bandwidth timing failed." << std::endl;
            return false;
        }
        samples.push_back(sample);
    }
    std::free(allocation);

    std::sort(samples.begin(), samples.end(),
        [](const BandwidthSample &left, const BandwidthSample &right) {
            return left.gb_per_second < right.gb_per_second;
        });
    const BandwidthSample &median = samples[samples.size() / 2];
    const double minimum = samples.front().gb_per_second;
    const double maximum = samples.back().gb_per_second;

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

    row[0] = std::to_string(cpu);
    row[1] = cpufb::format_cache_capacity(requested_bytes);
    row[2] = kernel.name;
    row[3] = format_decimal(median.gb_per_second, 3);
    row[4] = median.bytes_per_cycle > 0.0
        ? format_decimal(median.bytes_per_cycle, 3)
        : "-";
    row[5] = median.load_ipc > 0.0
        ? format_decimal(median.load_ipc, 3)
        : "-";
    row[6] = format_decimal(minimum, 3);
    row[7] = format_decimal(maximum, 3);
    row[8] = cycle_source;
    row[9] = workset_source;
    table.addOneItem(row);

    std::cout << "Single-core memory bandwidth: one sequential read stream, "
              << options.memory_repetitions << " sample(s)" << std::endl;
    table.print();

    vector<std::pair<string, const Table *> > sections;
    sections.push_back(std::make_pair(string("memory_bandwidth"), &table));
    return save_table_sections(options.save, sections);
}
