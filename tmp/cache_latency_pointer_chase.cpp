// Linux/AArch64 cache and DRAM load-to-use latency experiment.
//
// Build:
//   g++ -O3 -std=c++17 -march=armv8.2-a tmp/cache_latency_pointer_chase.cpp \
//       -o /tmp/cache_latency_pointer_chase
// Run on one isolated CPU in a single NUMA domain:
//   /tmp/cache_latency_pointer_chase --cpu=96 --freq-ghz=3.3
//
// The linked-list successor is randomized at cache-line granularity.  Each
// ldr consumes the pointer returned by the previous ldr, so neither the CPU's
// normal stream prefetcher nor software prefetch can create MLP to hide the
// response latency.  Results are therefore effective dependent-load latency,
// not bandwidth or a simple load instruction's execution latency.

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#if !defined(__linux__) || !defined(__aarch64__)
#error "This standalone experiment requires Linux/AArch64."
#endif

namespace {

constexpr std::uint64_t kKiB = 1024ULL;
constexpr std::uint64_t kMiB = 1024ULL * kKiB;
constexpr std::uint64_t kGiB = 1024ULL * kMiB;
constexpr std::size_t kCacheLineBytes = 64;
constexpr std::uint64_t kTransparentHugePageBytes = 2 * kMiB;
constexpr std::size_t kDependentLoadsPerBlock = 16;
constexpr std::uint64_t kDefaultLoadsPerSample = 8ULL * 1024 * 1024;
constexpr int kDefaultRepetitions = 7;

struct Options
{
    int cpu = 96;
    double frequency_ghz = 3.3;
    std::uint64_t loads_per_sample = kDefaultLoadsPerSample;
    int repetitions = kDefaultRepetitions;
    bool include_2g_dram = true;
};

struct Workset
{
    const char *name;
    std::uint64_t bytes;
    const char *expectation;
};

struct Sample
{
    double cycles_per_load = 0.0;
    double ns_per_load = 0.0;
    double seconds = 0.0;
};

struct Result
{
    Workset workset{};
    Sample median{};
    Sample minimum{};
    Sample maximum{};
    std::uint64_t huge_page_bytes = 0;
    std::uint64_t allocation_bytes = 0;
};

class CycleCounter
{
public:
    CycleCounter() : fd_(-1)
    {
        perf_event_attr event{};
        event.type = PERF_TYPE_HARDWARE;
        event.size = sizeof(event);
        event.config = PERF_COUNT_HW_CPU_CYCLES;
        event.disabled = 1;
        event.exclude_kernel = 1;
        event.exclude_hv = 1;
        fd_ = static_cast<int>(syscall(__NR_perf_event_open, &event, 0, -1,
            -1, 0));
    }

    ~CycleCounter()
    {
        if (fd_ >= 0) close(fd_);
    }

    bool available() const { return fd_ >= 0; }

    bool start()
    {
        return fd_ >= 0 && ioctl(fd_, PERF_EVENT_IOC_RESET, 0) == 0 &&
            ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0) == 0;
    }

    std::uint64_t stop()
    {
        if (fd_ < 0 || ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0) != 0) return 0;
        std::uint64_t cycles = 0;
        return read(fd_, &cycles, sizeof(cycles)) ==
                static_cast<ssize_t>(sizeof(cycles)) ?
            cycles : 0;
    }

private:
    int fd_;
};

double elapsed_seconds(const timespec &start, const timespec &end)
{
    return static_cast<double>(end.tv_sec - start.tv_sec) +
        static_cast<double>(end.tv_nsec - start.tv_nsec) * 1.0e-9;
}

bool pin_to_cpu(int cpu)
{
    if (cpu < 0 || cpu >= CPU_SETSIZE) return false;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    return sched_setaffinity(0, sizeof(mask), &mask) == 0;
}

bool parse_unsigned(const std::string &text, std::uint64_t &value)
{
    if (text.empty()) return false;
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') return false;
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_double_positive(const std::string &text, double &value)
{
    if (text.empty()) return false;
    errno = 0;
    char *end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
        !std::isfinite(parsed) || parsed <= 0.0) {
        return false;
    }
    value = parsed;
    return true;
}

void print_usage(const char *program)
{
    std::cerr << "Usage: " << program << " [options]\n"
              << "  --cpu=N              pinned Linux CPU (default: 96)\n"
              << "  --freq-ghz=F         cycle-counter frequency for ns (default: 3.3)\n"
              << "  --loads=N            dependent loads/sample (default: 8388608)\n"
              << "  --repetitions=N      samples/workset (default: 7)\n"
              << "  --skip-2g            skip the 2 GiB DRAM-dominated point\n";
}

bool parse_options(int argc, char **argv, Options &options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--skip-2g") {
            options.include_2g_dram = false;
            continue;
        }
        const std::size_t equals = argument.find('=');
        if (equals == std::string::npos) return false;
        const std::string name = argument.substr(0, equals);
        const std::string value = argument.substr(equals + 1);
        std::uint64_t parsed = 0;
        if (name == "--cpu") {
            if (!parse_unsigned(value, parsed) || parsed >= CPU_SETSIZE)
                return false;
            options.cpu = static_cast<int>(parsed);
        } else if (name == "--freq-ghz") {
            if (!parse_double_positive(value, options.frequency_ghz))
                return false;
        } else if (name == "--loads") {
            if (!parse_unsigned(value, parsed) || parsed == 0) return false;
            options.loads_per_sample = parsed;
        } else if (name == "--repetitions") {
            if (!parse_unsigned(value, parsed) || parsed == 0 ||
                parsed > static_cast<std::uint64_t>(INT_MAX)) {
                return false;
            }
            options.repetitions = static_cast<int>(parsed);
        } else {
            return false;
        }
    }
    options.loads_per_sample -= options.loads_per_sample %
        kDependentLoadsPerBlock;
    return options.loads_per_sample >= kDependentLoadsPerBlock;
}

// The chain has 16 serial dependent loads per loop branch.  The independent
// decrement/branch can execute while the preceding ldr is outstanding, so its
// contribution is negligible and intentionally not subtracted.
__attribute__((noinline)) std::uintptr_t chase_chain(std::uintptr_t pointer,
    std::uint64_t blocks)
{
    asm volatile(
        "1:\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "ldr %0, [%0]\n\t"
        "subs %1, %1, #1\n\t"
        "b.ne 1b\n\t"
        : "+r"(pointer), "+r"(blocks)
        :
        : "cc", "memory");
    return pointer;
}

std::uintptr_t node_address(std::uint8_t *base, std::uint64_t line)
{
    return reinterpret_cast<std::uintptr_t>(base + line * kCacheLineBytes);
}

bool build_random_cycle(std::uint8_t *base, std::uint64_t bytes,
    std::uint64_t seed, std::uintptr_t &start)
{
    const std::uint64_t lines = bytes / kCacheLineBytes;
    if (lines < 2 || lines > std::numeric_limits<std::uint32_t>::max())
        return false;

    std::vector<std::uint32_t> order(static_cast<std::size_t>(lines));
    std::iota(order.begin(), order.end(), 0U);
    std::mt19937_64 random(seed);
    std::shuffle(order.begin(), order.end(), random);
    for (std::uint64_t index = 0; index < lines; ++index) {
        const std::uint64_t next = (index + 1) % lines;
        std::uintptr_t *const node = reinterpret_cast<std::uintptr_t *>(
            node_address(base, order[static_cast<std::size_t>(index)]));
        *node = node_address(base, order[static_cast<std::size_t>(next)]);
    }
    start = node_address(base, order.front());
    return true;
}

std::uint64_t read_anon_huge_page_bytes(const void *address)
{
    std::ifstream input("/proc/self/smaps");
    if (!input) return 0;

    const std::uintptr_t target = reinterpret_cast<std::uintptr_t>(address);
    bool in_mapping = false;
    std::string line;
    while (std::getline(input, line)) {
        std::uintptr_t start = 0;
        std::uintptr_t end = 0;
        if (std::sscanf(line.c_str(), "%lx-%lx", &start, &end) == 2) {
            in_mapping = target >= start && target < end;
            continue;
        }
        if (!in_mapping || line.rfind("AnonHugePages:", 0) != 0) continue;
        std::istringstream stream(line.substr(std::strlen("AnonHugePages:")));
        std::uint64_t kib = 0;
        if (stream >> kib) return kib * kKiB;
    }
    return 0;
}

Sample measure_chain(std::uintptr_t start, std::uint64_t loads,
    CycleCounter &counter, double frequency_ghz)
{
    const std::uint64_t blocks = loads / kDependentLoadsPerBlock;
    timespec wall_start{};
    timespec wall_end{};
    if (!counter.start()) return {};
    clock_gettime(CLOCK_MONOTONIC_RAW, &wall_start);
    const std::uintptr_t final = chase_chain(start, blocks);
    clock_gettime(CLOCK_MONOTONIC_RAW, &wall_end);
    const std::uint64_t cycles = counter.stop();
    // Make the result architecturally observable to avoid an accidental future
    // refactor weakening the inline-assembly barrier.
    asm volatile("" : : "r"(final) : "memory");

    Sample sample;
    sample.seconds = elapsed_seconds(wall_start, wall_end);
    sample.cycles_per_load = static_cast<double>(cycles) / loads;
    sample.ns_per_load = sample.cycles_per_load / frequency_ghz;
    return sample;
}

bool run_workset(const Workset &workset, const Options &options,
    std::uint64_t seed, Result &result)
{
    if (workset.bytes % kCacheLineBytes != 0 || workset.bytes < 2 * kCacheLineBytes)
        return false;
    if (workset.bytes > std::numeric_limits<std::uint64_t>::max() -
        (kTransparentHugePageBytes - 1)) {
        return false;
    }
    const std::uint64_t allocation_bytes =
        (workset.bytes + kTransparentHugePageBytes - 1) /
        kTransparentHugePageBytes * kTransparentHugePageBytes;
    if (allocation_bytes > std::numeric_limits<std::size_t>::max()) return false;

    void *mapping = mmap(nullptr, static_cast<std::size_t>(allocation_bytes),
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        std::cerr << "mmap(" << workset.bytes / kMiB << " MiB): "
                  << std::strerror(errno) << std::endl;
        return false;
    }
    std::uint8_t *const data = static_cast<std::uint8_t *>(mapping);
    (void)madvise(data, static_cast<std::size_t>(allocation_bytes),
        MADV_HUGEPAGE);
    // Fully fault the rounded mapping so even the sub-2 MiB L1/L2 active
    // ranges receive a transparent huge page when the kernel permits it.
    std::memset(data, 0, static_cast<std::size_t>(allocation_bytes));

    std::uintptr_t start = 0;
    if (!build_random_cycle(data, workset.bytes, seed, start)) {
        munmap(mapping, static_cast<std::size_t>(allocation_bytes));
        return false;
    }
    const std::uint64_t lines = workset.bytes / kCacheLineBytes;
    const std::uint64_t warm_loads = lines - lines % kDependentLoadsPerBlock;
    // Fill the intended hierarchy before recording; for worksets larger than a
    // level this creates the steady-state eviction pattern of a random chain.
    (void)chase_chain(start, warm_loads / kDependentLoadsPerBlock);

    CycleCounter counter;
    if (!counter.available()) {
        std::cerr << "perf CPU-cycle counter is unavailable; cannot report "
                  << "cycle-normalized latency." << std::endl;
        munmap(mapping, static_cast<std::size_t>(allocation_bytes));
        return false;
    }

    std::vector<Sample> samples;
    samples.reserve(options.repetitions);
    for (int repetition = 0; repetition < options.repetitions; ++repetition) {
        Sample sample = measure_chain(start, options.loads_per_sample, counter,
            options.frequency_ghz);
        if (sample.cycles_per_load <= 0.0 || !std::isfinite(sample.cycles_per_load)) {
            munmap(mapping, static_cast<std::size_t>(allocation_bytes));
            return false;
        }
        samples.push_back(sample);
    }
    std::sort(samples.begin(), samples.end(),
        [](const Sample &left, const Sample &right) {
            return left.cycles_per_load < right.cycles_per_load;
        });

    result.workset = workset;
    result.minimum = samples.front();
    result.median = samples[samples.size() / 2];
    result.maximum = samples.back();
    result.allocation_bytes = allocation_bytes;
    result.huge_page_bytes = read_anon_huge_page_bytes(data);
    munmap(mapping, static_cast<std::size_t>(allocation_bytes));
    return true;
}

std::string format_bytes(std::uint64_t bytes)
{
    if (bytes % kGiB == 0) return std::to_string(bytes / kGiB) + " GiB";
    if (bytes % kMiB == 0) return std::to_string(bytes / kMiB) + " MiB";
    if (bytes % kKiB == 0) return std::to_string(bytes / kKiB) + " KiB";
    return std::to_string(bytes) + " B";
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!pin_to_cpu(options.cpu)) {
        std::cerr << "sched_setaffinity(" << options.cpu << "): "
                  << std::strerror(errno) << std::endl;
        return 1;
    }

    std::vector<Workset> worksets = {
        {"L1-resident", 32 * kKiB, "below 64 KiB L1D"},
        {"L2-resident", 1 * kMiB, "above L1D, below 2 MiB L2"},
        {"L3-resident", 48 * kMiB, "above L2, below 96 MiB shared L3"},
        {"DRAM-dominated", 1 * kGiB, "10.7 x L3; residual L3 hits possible"},
    };
    if (options.include_2g_dram) {
        worksets.push_back({"DRAM-dominated-2G", 2 * kGiB,
            "21.3 x L3; fewer residual L3 hits"});
    }

    std::cout << "Pinned CPU: " << options.cpu << "; PMU: CPU cycles; "
              << "frequency for ns conversion: " << options.frequency_ghz
              << " GHz\n";
    std::cout << "Dependent random linked-list: " << kDependentLoadsPerBlock
              << " serial ldr/block; " << options.loads_per_sample
              << " loads/sample; " << options.repetitions << " samples\n";
    std::cout << "Level,Workset,Median cycles/load,Median ns/load,"
              << "Min/Max cycles/load,THP coverage,Interpretation\n";

    std::uint64_t seed = 0x2e70a9c9ULL;
    for (const Workset &workset : worksets) {
        Result result;
        if (!run_workset(workset, options, seed++, result)) return 1;
        const std::uint64_t active_huge_bytes = std::min(
            result.huge_page_bytes, result.workset.bytes);
        const double huge_ratio = result.workset.bytes == 0 ? 0.0 :
            100.0 * static_cast<double>(active_huge_bytes) /
            result.workset.bytes;
        std::cout << result.workset.name << ','
                  << format_bytes(result.workset.bytes) << ','
                  << std::fixed << std::setprecision(2)
                  << result.median.cycles_per_load << ','
                  << result.median.ns_per_load << ','
                  << result.minimum.cycles_per_load << '/'
                  << result.maximum.cycles_per_load << ','
                  << std::setprecision(1) << huge_ratio << "% (" <<
                  format_bytes(result.huge_page_bytes) << ")," <<
                  result.workset.expectation << '\n';
    }
    return 0;
}
