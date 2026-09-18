// AArch64 cache/memory load-saturation and access-pattern experiment.
//
// macOS:
//   c++ -O3 -std=c++17 -pthread tmp/load_saturation_sweep.cpp -o /tmp/load_saturation_sweep
//   /tmp/load_saturation_sweep --cpus=0 --levels=l1,l2 --repetitions=5 --target-ms=20
//
// Linux:
//   g++ -O3 -std=c++17 -march=armv8-a -pthread tmp/load_saturation_sweep.cpp -o /tmp/load_saturation_sweep
//   CPUFB_FREQ_GHZ=3.3 /tmp/load_saturation_sweep --cpus=96-119 --levels=l1,l2,l3,mem --repetitions=7 --target-ms=30
//
// The envelope section varies per-stream unroll, independent dense streams,
// and a serial integer delay after each load group.  It reports the smallest
// observed load IPC that still sustains at least 95% of the best dense-load
// bandwidth.  The pattern section keeps the same 16-byte NEON ld1 instruction
// while changing cache-line utilization and address order.  Random rows use
// 1/2/4/8/16 independent scalar pointer chains to expose the MLP requirement.

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <sched.h>
#include <sys/ioctl.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#if !defined(__aarch64__)
#error "This experiment requires AArch64."
#endif

namespace {

constexpr std::uint64_t kKiB = 1024ULL;
constexpr std::uint64_t kMiB = 1024ULL * kKiB;
constexpr std::uint64_t kGiB = 1024ULL * kMiB;
constexpr std::size_t kVectorBytes = 16;
constexpr double kDefaultTargetMilliseconds = 20.0;
constexpr std::uint64_t kMaximumPasses = 1ULL << 32;

enum class LevelKind
{
    L1,
    L2,
    L3,
    Memory,
};

enum class PatternKind
{
    Dense,
    Line,
    Stride2,
    Stride4,
    Page,
    Multi,
    Random,
};

enum class KernelKind
{
    Dense,
    Strided,
    Random,
};

struct Options
{
    std::vector<int> cpus;
    std::set<LevelKind> levels = {
        LevelKind::L1,
        LevelKind::L2,
        LevelKind::L3,
        LevelKind::Memory,
    };
    std::set<PatternKind> patterns = {
        PatternKind::Dense,
        PatternKind::Line,
        PatternKind::Stride2,
        PatternKind::Stride4,
        PatternKind::Page,
        PatternKind::Multi,
        PatternKind::Random,
    };
    bool run_envelope = true;
    bool run_patterns = true;
    int repetitions = 5;
    double target_milliseconds = kDefaultTargetMilliseconds;
    std::uint64_t memory_bytes_per_worker = 0;
    double frequency_hz = 0.0;
};

struct CacheInfo
{
    std::uint64_t bytes = 0;
    std::string source;
};

struct Topology
{
    CacheInfo l1;
    CacheInfo l2;
    CacheInfo l3;
#if defined(__APPLE__)
    // Apple Silicon uses 128-byte cache lines.  Some sandboxed macOS
    // environments expose cache capacities but reject hw.cachelinesize.
    std::uint64_t line_bytes = 128;
    std::string line_source = "128-byte Apple Silicon fallback";
#else
    std::uint64_t line_bytes = 64;
    std::string line_source = "64-byte AArch64 fallback";
#endif
};

struct LevelSpec
{
    LevelKind kind = LevelKind::L1;
    std::string name;
    std::uint64_t bytes_per_worker = 0;
    std::string source;
};

struct Case
{
    std::string section;
    std::string level;
    std::string pattern;
    KernelKind kernel = KernelKind::Dense;
    std::uint64_t workset_bytes = 0;
    std::uint64_t stride_bytes = kVectorBytes;
    int streams = 1;
    int unroll = 1;
    int delay_iterations = 0;
    int random_chains = 0;
    std::uint64_t loads_per_pass = 0;
    std::uint64_t useful_bytes_per_pass = 0;
    std::uint64_t line_bytes_per_pass = 0;
    std::uint64_t useful_bytes_per_load = kVectorBytes;
};

struct WorkerRecord
{
    timespec start{};
    timespec end{};
    std::uint64_t cycles = 0;
};

struct Sample
{
    double seconds = 0.0;
    double useful_gb_per_second = 0.0;
    double line_gb_per_second = 0.0;
    double load_ipc = 0.0;
    double load_gips_per_core = 0.0;
};

struct Result
{
    Case test_case;
    std::uint64_t passes = 1;
    Sample median;
    double minimum_line_gb_per_second = 0.0;
    double maximum_line_gb_per_second = 0.0;
    std::string cycle_source;
};

double elapsed_seconds(const timespec &start, const timespec &end)
{
    return static_cast<double>(end.tv_sec - start.tv_sec) +
        static_cast<double>(end.tv_nsec - start.tv_nsec) * 1.0e-9;
}

bool earlier(const timespec &left, const timespec &right)
{
    return left.tv_sec < right.tv_sec ||
        (left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::string format_bytes(std::uint64_t bytes)
{
    if (bytes % kGiB == 0) return std::to_string(bytes / kGiB) + " GiB";
    if (bytes % kMiB == 0) return std::to_string(bytes / kMiB) + " MiB";
    if (bytes % kKiB == 0) return std::to_string(bytes / kKiB) + " KiB";
    return std::to_string(bytes) + " B";
}

std::uint64_t align_down(std::uint64_t value, std::uint64_t alignment)
{
    return alignment == 0 ? value : value - value % alignment;
}

std::uint64_t greatest_power_of_two(std::uint64_t value)
{
    if (value == 0) return 0;
    std::uint64_t result = 1;
    while (result <= value / 2) result <<= 1;
    return result;
}

bool parse_unsigned(const std::string &text, std::uint64_t &value,
    bool allow_zero = false)
{
    if (text.empty()) return false;
    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
        (!allow_zero && parsed == 0)) {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_double(const std::string &text, double &value)
{
    if (text.empty()) return false;
    char *end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
        !std::isfinite(parsed) || parsed <= 0.0) {
        return false;
    }
    value = parsed;
    return true;
}

std::vector<std::string> split(const std::string &text)
{
    std::vector<std::string> result;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t end = text.find(',', begin);
        const std::string item = text.substr(begin, end - begin);
        if (item.empty()) return std::vector<std::string>();
        result.push_back(lower(item));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return result;
}

bool parse_cpus(const std::string &text, std::vector<int> &cpus)
{
    cpus.clear();
    std::set<int> seen;
    for (const std::string &item : split(text)) {
        const std::size_t dash = item.find('-');
        std::uint64_t first = 0;
        std::uint64_t last = 0;
        if (dash == std::string::npos) {
            if (!parse_unsigned(item, first, true)) return false;
            last = first;
        } else {
            if (!parse_unsigned(item.substr(0, dash), first, true) ||
                !parse_unsigned(item.substr(dash + 1), last, true) ||
                first > last) {
                return false;
            }
        }
        if (last > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            return false;
        for (std::uint64_t cpu = first; cpu <= last; ++cpu) {
            if (seen.insert(static_cast<int>(cpu)).second)
                cpus.push_back(static_cast<int>(cpu));
        }
    }
    return !cpus.empty();
}

bool parse_levels(const std::string &text, std::set<LevelKind> &levels)
{
    levels.clear();
    for (const std::string &item : split(text)) {
        if (item == "l1") levels.insert(LevelKind::L1);
        else if (item == "l2") levels.insert(LevelKind::L2);
        else if (item == "l3") levels.insert(LevelKind::L3);
        else if (item == "mem" || item == "memory" || item == "dram")
            levels.insert(LevelKind::Memory);
        else return false;
    }
    return !levels.empty();
}

bool parse_patterns(const std::string &text,
    std::set<PatternKind> &patterns)
{
    patterns.clear();
    for (const std::string &item : split(text)) {
        if (item == "dense") patterns.insert(PatternKind::Dense);
        else if (item == "line") patterns.insert(PatternKind::Line);
        else if (item == "stride2" || item == "2cl")
            patterns.insert(PatternKind::Stride2);
        else if (item == "stride4" || item == "4cl")
            patterns.insert(PatternKind::Stride4);
        else if (item == "page" || item == "4k")
            patterns.insert(PatternKind::Page);
        else if (item == "multi" || item == "multistream")
            patterns.insert(PatternKind::Multi);
        else if (item == "random" || item == "pointer")
            patterns.insert(PatternKind::Random);
        else return false;
    }
    return !patterns.empty();
}

bool parse_sections(const std::string &text, Options &options)
{
    options.run_envelope = false;
    options.run_patterns = false;
    for (const std::string &item : split(text)) {
        if (item == "envelope") options.run_envelope = true;
        else if (item == "patterns") options.run_patterns = true;
        else return false;
    }
    return options.run_envelope || options.run_patterns;
}

int default_cpu()
{
#if defined(__linux__)
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &mask)) return cpu;
        }
    }
#endif
    return 0;
}

void print_usage(const char *program)
{
    std::cerr
        << "Usage: " << program << " [options]\n"
        << "  --cpus=0,2-4                 Worker CPUs (default: first allowed CPU)\n"
        << "  --levels=l1,l2,l3,mem        Target hierarchy levels\n"
        << "  --sections=envelope,patterns Run experiment 1 and/or 2\n"
        << "  --patterns=dense,line,stride2,stride4,page,multi,random\n"
        << "  --repetitions=5              Samples per case\n"
        << "  --target-ms=20               Calibrated duration per sample\n"
        << "  --memory-mib=N               DRAM workset per worker (default: auto)\n"
        << "  --freq-ghz=F                 Cycle fallback when PMU is unavailable\n";
}

bool parse_options(int argc, char **argv, Options &options)
{
    options.cpus.push_back(default_cpu());
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        }
        const std::size_t equals = argument.find('=');
        if (equals == std::string::npos) return false;
        const std::string name = argument.substr(0, equals);
        const std::string value = argument.substr(equals + 1);
        std::uint64_t parsed = 0;
        double parsed_double = 0.0;
        if (name == "--cpus") {
            if (!parse_cpus(value, options.cpus)) return false;
        } else if (name == "--levels") {
            if (!parse_levels(value, options.levels)) return false;
        } else if (name == "--sections") {
            if (!parse_sections(value, options)) return false;
        } else if (name == "--patterns") {
            if (!parse_patterns(value, options.patterns)) return false;
        } else if (name == "--repetitions") {
            if (!parse_unsigned(value, parsed) || parsed > 1000) return false;
            options.repetitions = static_cast<int>(parsed);
        } else if (name == "--target-ms") {
            if (!parse_double(value, parsed_double)) return false;
            options.target_milliseconds = parsed_double;
        } else if (name == "--memory-mib") {
            if (!parse_unsigned(value, parsed) ||
                parsed > std::numeric_limits<std::uint64_t>::max() / kMiB) {
                return false;
            }
            options.memory_bytes_per_worker = parsed * kMiB;
        } else if (name == "--freq-ghz") {
            if (!parse_double(value, parsed_double)) return false;
            options.frequency_hz = parsed_double * 1.0e9;
        } else {
            return false;
        }
    }
    if (options.frequency_hz == 0.0) {
        const char *environment = std::getenv("CPUFB_FREQ_GHZ");
        double ghz = 0.0;
        if (environment != nullptr && parse_double(environment, ghz))
            options.frequency_hz = ghz * 1.0e9;
    }
    return !options.cpus.empty();
}

#if defined(__linux__)
std::uint64_t parse_capacity(std::string text)
{
    if (text.empty()) return 0;
    while (!text.empty() && std::isspace(
            static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    std::uint64_t multiplier = 1;
    if (!text.empty()) {
        const char suffix = static_cast<char>(std::tolower(text.back()));
        if (suffix == 'k' || suffix == 'm' || suffix == 'g') {
            multiplier = suffix == 'k' ? kKiB : suffix == 'm' ? kMiB : kGiB;
            text.pop_back();
        }
    }
    std::uint64_t value = 0;
    return parse_unsigned(text, value) &&
        value <= std::numeric_limits<std::uint64_t>::max() / multiplier
        ? value * multiplier : 0;
}

bool read_text_file(const std::string &path, std::string &value)
{
    std::ifstream input(path.c_str());
    if (!input) return false;
    std::getline(input, value);
    return !value.empty();
}

CacheInfo detect_linux_cache(int cpu, int wanted_level)
{
    CacheInfo result;
    for (int index = 0; index < 32; ++index) {
        const std::string base = "/sys/devices/system/cpu/cpu" +
            std::to_string(cpu) + "/cache/index" + std::to_string(index) + "/";
        std::string level;
        std::string type;
        std::string size;
        if (!read_text_file(base + "level", level)) continue;
        if (std::atoi(level.c_str()) != wanted_level ||
            !read_text_file(base + "type", type) ||
            (lower(type) != "data" && lower(type) != "unified") ||
            !read_text_file(base + "size", size)) {
            continue;
        }
        const std::uint64_t bytes = parse_capacity(size);
        if (bytes > result.bytes) {
            result.bytes = bytes;
            result.source = base + "size";
        }
    }
    return result;
}

std::uint64_t detect_linux_line_size(int cpu, std::string &source)
{
    for (int index = 0; index < 32; ++index) {
        const std::string path = "/sys/devices/system/cpu/cpu" +
            std::to_string(cpu) + "/cache/index" + std::to_string(index) +
            "/coherency_line_size";
        std::string text;
        std::uint64_t value = 0;
        if (read_text_file(path, text) && parse_unsigned(text, value) &&
            value >= kVectorBytes && (value & (value - 1)) == 0) {
            source = path;
            return value;
        }
    }
    return 0;
}
#endif

#if defined(__APPLE__)
std::uint64_t read_sysctl_u64(const char *name)
{
    std::uint64_t value = 0;
    std::size_t size = sizeof(value);
    return sysctlbyname(name, &value, &size, nullptr, 0) == 0 ? value : 0;
}

CacheInfo detect_macos_cache(int level)
{
    CacheInfo result;
    const char *keys[3] = {nullptr, nullptr, nullptr};
    if (level == 1) {
        keys[0] = "hw.perflevel0.l1dcachesize";
        keys[1] = "hw.l1dcachesize";
    } else if (level == 2) {
        keys[0] = "hw.perflevel0.l2cachesize";
        keys[1] = "hw.l2cachesize";
    } else if (level == 3) {
        keys[0] = "hw.perflevel0.l3cachesize";
        keys[1] = "hw.l3cachesize";
    }
    for (const char *key : keys) {
        if (key == nullptr) continue;
        const std::uint64_t value = read_sysctl_u64(key);
        if (value > result.bytes) {
            result.bytes = value;
            result.source = std::string("sysctl ") + key;
        }
    }
    return result;
}
#endif

Topology detect_topology(int cpu)
{
    Topology result;
#if defined(__linux__)
    result.l1 = detect_linux_cache(cpu, 1);
    result.l2 = detect_linux_cache(cpu, 2);
    result.l3 = detect_linux_cache(cpu, 3);
    const std::uint64_t line = detect_linux_line_size(cpu, result.line_source);
    if (line > 0) result.line_bytes = line;
#elif defined(__APPLE__)
    (void)cpu;
    result.l1 = detect_macos_cache(1);
    result.l2 = detect_macos_cache(2);
    result.l3 = detect_macos_cache(3);
    const std::uint64_t line = read_sysctl_u64("hw.cachelinesize");
    if (line >= kVectorBytes && (line & (line - 1)) == 0) {
        result.line_bytes = line;
        result.line_source = "sysctl hw.cachelinesize";
    }
#endif
    if (result.l1.bytes == 0) {
        result.l1.bytes = 64 * kKiB;
        result.l1.source = "64 KiB fallback";
    }
    if (result.l2.bytes == 0) {
        result.l2.bytes = 1024 * kKiB;
        result.l2.source = "1 MiB fallback";
    }
    return result;
}

std::vector<LevelSpec> select_level_specs(const Options &options,
    const Topology &topology)
{
    std::vector<LevelSpec> result;
    const std::uint64_t line = topology.line_bytes;
    const std::uint64_t worker_count = options.cpus.size();

    if (options.levels.count(LevelKind::L1) != 0) {
        LevelSpec spec;
        spec.kind = LevelKind::L1;
        spec.name = "L1";
        spec.bytes_per_worker = align_down(topology.l1.bytes / 2, line);
        spec.source = "1/2 of " + format_bytes(topology.l1.bytes) +
            " from " + topology.l1.source;
        if (spec.bytes_per_worker >= line) result.push_back(spec);
    }

    if (options.levels.count(LevelKind::L2) != 0) {
        LevelSpec spec;
        spec.kind = LevelKind::L2;
        spec.name = "L2";
        std::uint64_t candidate = std::max(topology.l2.bytes / 2,
            topology.l1.bytes * 2);
        candidate = std::min(candidate,
            topology.l2.bytes - topology.l2.bytes / 4);
        spec.bytes_per_worker = align_down(candidate, line);
        spec.source = "L1 < workset < L2; " + topology.l2.source;
        if (spec.bytes_per_worker > topology.l1.bytes)
            result.push_back(spec);
    }

    if (options.levels.count(LevelKind::L3) != 0) {
        if (topology.l3.bytes == 0) {
            std::cerr << "Warning: L3 was requested but is not reported; "
                      << "skipping L3.\n";
        } else {
            LevelSpec spec;
            spec.kind = LevelKind::L3;
            spec.name = "L3";
            const std::uint64_t aggregate =
                topology.l3.bytes - topology.l3.bytes / 4;
            spec.bytes_per_worker = align_down(aggregate / worker_count, line);
            spec.source = "3/4 of shared " + format_bytes(topology.l3.bytes) +
                " split across workers; " + topology.l3.source;
            if (spec.bytes_per_worker > topology.l2.bytes) {
                result.push_back(spec);
            } else {
                std::cerr << "Warning: L3 share "
                          << format_bytes(spec.bytes_per_worker)
                          << "/worker does not exceed private L2; skipping L3.\n";
            }
        }
    }

    if (options.levels.count(LevelKind::Memory) != 0) {
        LevelSpec spec;
        spec.kind = LevelKind::Memory;
        spec.name = "Memory";
        if (options.memory_bytes_per_worker > 0) {
            spec.bytes_per_worker = align_down(
                options.memory_bytes_per_worker, line);
            spec.source = "CLI --memory-mib per worker";
        } else {
            std::uint64_t candidate = std::max<std::uint64_t>(64 * kMiB,
                topology.l2.bytes * 8);
            if (topology.l3.bytes > 0) {
                const std::uint64_t four_l3_per_worker =
                    topology.l3.bytes <=
                        std::numeric_limits<std::uint64_t>::max() / 4
                    ? (topology.l3.bytes * 4 + worker_count - 1) / worker_count
                    : std::numeric_limits<std::uint64_t>::max() / worker_count;
                candidate = std::max(candidate, four_l3_per_worker);
            }
            spec.bytes_per_worker = align_down(candidate, line);
            spec.source = "auto: max(64 MiB, 8 x L2, 4 x L3 / workers)";
        }
        if (spec.bytes_per_worker >= line) result.push_back(spec);
    }
    return result;
}

bool pin_current_thread(int cpu, std::string &error)
{
#if defined(__linux__)
    if (cpu < 0 || cpu >= CPU_SETSIZE) {
        error = "CPU " + std::to_string(cpu) + " is outside CPU_SETSIZE";
        return false;
    }
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
        error = "sched_setaffinity(" + std::to_string(cpu) + "): " +
            std::strerror(errno);
        return false;
    }
#else
    (void)cpu;
    (void)error;
#endif
    return true;
}

double read_cpu_frequency_hz(int cpu)
{
#if defined(__linux__)
    static const char *const names[] = {
        "scaling_cur_freq",
        "cpuinfo_cur_freq",
        "scaling_max_freq",
        "cpuinfo_max_freq",
    };
    const std::string base = "/sys/devices/system/cpu/cpu" +
        std::to_string(cpu) + "/cpufreq/";
    for (const char *name : names) {
        std::ifstream input((base + name).c_str());
        double khz = 0.0;
        if (input >> khz && khz > 0.0) return khz * 1000.0;
    }
#elif defined(__APPLE__)
    (void)cpu;
    const std::uint64_t frequency = read_sysctl_u64("hw.cpufrequency");
    if (frequency > 0) return static_cast<double>(frequency);
#else
    (void)cpu;
#endif
    return 0.0;
}

class CycleCounter
{
public:
    CycleCounter() : fd_(-1)
    {
#if defined(__linux__)
        perf_event_attr event;
        std::memset(&event, 0, sizeof(event));
        event.type = PERF_TYPE_HARDWARE;
        event.size = sizeof(event);
        event.config = PERF_COUNT_HW_CPU_CYCLES;
        event.disabled = 1;
        event.exclude_kernel = 1;
        event.exclude_hv = 1;
        fd_ = static_cast<int>(syscall(__NR_perf_event_open,
            &event, 0, -1, -1, 0));
#endif
    }

    ~CycleCounter()
    {
        if (fd_ >= 0) close(fd_);
    }

    bool start()
    {
#if defined(__linux__)
        return fd_ >= 0 && ioctl(fd_, PERF_EVENT_IOC_RESET, 0) == 0 &&
            ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0) == 0;
#else
        return false;
#endif
    }

    std::uint64_t stop()
    {
#if defined(__linux__)
        if (fd_ < 0 || ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0) != 0) return 0;
        std::uint64_t value = 0;
        return read(fd_, &value, sizeof(value)) ==
            static_cast<ssize_t>(sizeof(value)) ? value : 0;
#else
        return 0;
#endif
    }

private:
    int fd_;
};

template<int Register>
inline void load_vector(const void *address);

#define DEFINE_VECTOR_LOAD(register_number)                                  \
    template<>                                                               \
    inline void load_vector<register_number>(const void *address)            \
    {                                                                         \
        asm volatile("ld1 {v" #register_number ".8h}, [%0]"                \
            : : "r"(address) : "v" #register_number);                    \
    }

DEFINE_VECTOR_LOAD(0)
DEFINE_VECTOR_LOAD(1)
DEFINE_VECTOR_LOAD(2)
DEFINE_VECTOR_LOAD(3)
DEFINE_VECTOR_LOAD(4)
DEFINE_VECTOR_LOAD(5)
DEFINE_VECTOR_LOAD(6)
DEFINE_VECTOR_LOAD(7)
DEFINE_VECTOR_LOAD(8)
DEFINE_VECTOR_LOAD(9)
DEFINE_VECTOR_LOAD(10)
DEFINE_VECTOR_LOAD(11)
DEFINE_VECTOR_LOAD(12)
DEFINE_VECTOR_LOAD(13)
DEFINE_VECTOR_LOAD(14)
DEFINE_VECTOR_LOAD(15)

#undef DEFINE_VECTOR_LOAD

inline void serial_delay(std::uint64_t iterations, std::uint64_t &state)
{
    if (iterations == 0) return;
    std::uint64_t count = iterations;
    asm volatile(
        "1:\n\t"
        "add %[state], %[state], #1\n\t"
        "subs %[count], %[count], #1\n\t"
        "b.ne 1b\n\t"
        : [state] "+r"(state), [count] "+r"(count)
        :
        : "cc");
}

template<int Index, int Total, int Streams>
struct DenseIssuer
{
    static inline void run(std::array<const std::uint8_t *, Streams> &pointers)
    {
        constexpr int stream = Index % Streams;
        constexpr int offset = (Index / Streams) * kVectorBytes;
        load_vector<Index % 16>(pointers[stream] + offset);
        DenseIssuer<Index + 1, Total, Streams>::run(pointers);
    }
};

template<int Total, int Streams>
struct DenseIssuer<Total, Total, Streams>
{
    static inline void run(std::array<const std::uint8_t *, Streams> &) {}
};

template<int Streams, int Unroll>
__attribute__((noinline))
void run_dense(const std::uint8_t *data, const Case &test_case,
    std::uint64_t passes)
{
    const std::uint64_t bytes_per_stream =
        test_case.workset_bytes / Streams;
    const std::uint64_t groups = bytes_per_stream /
        (static_cast<std::uint64_t>(Unroll) * kVectorBytes);
    std::uint64_t delay_state = reinterpret_cast<std::uintptr_t>(data);
    for (std::uint64_t pass = 0; pass < passes; ++pass) {
        std::array<const std::uint8_t *, Streams> pointers{};
        for (int stream = 0; stream < Streams; ++stream)
            pointers[stream] = data + bytes_per_stream * stream;
        for (std::uint64_t group = 0; group < groups; ++group) {
            DenseIssuer<0, Streams * Unroll, Streams>::run(pointers);
            for (int stream = 0; stream < Streams; ++stream)
                pointers[stream] += Unroll * kVectorBytes;
            serial_delay(static_cast<std::uint64_t>(
                test_case.delay_iterations), delay_state);
        }
    }
    asm volatile("" : : "r"(delay_state) : "memory");
}

template<int Streams>
void dispatch_dense_unroll(const std::uint8_t *data, const Case &test_case,
    std::uint64_t passes)
{
    switch (test_case.unroll) {
    case 1: run_dense<Streams, 1>(data, test_case, passes); break;
    case 2: run_dense<Streams, 2>(data, test_case, passes); break;
    case 4: run_dense<Streams, 4>(data, test_case, passes); break;
    case 8: run_dense<Streams, 8>(data, test_case, passes); break;
    case 16: run_dense<Streams, 16>(data, test_case, passes); break;
    default: std::abort();
    }
}

void dispatch_dense(const std::uint8_t *data, const Case &test_case,
    std::uint64_t passes)
{
    switch (test_case.streams) {
    case 1: dispatch_dense_unroll<1>(data, test_case, passes); break;
    case 2: dispatch_dense_unroll<2>(data, test_case, passes); break;
    case 4: dispatch_dense_unroll<4>(data, test_case, passes); break;
    case 8: dispatch_dense_unroll<8>(data, test_case, passes); break;
    default: std::abort();
    }
}

template<int Index, int Total>
struct StridedIssuer
{
    static inline void run(const std::uint8_t *base, std::uint64_t stride)
    {
        load_vector<Index % 16>(base + static_cast<std::uint64_t>(Index) * stride);
        StridedIssuer<Index + 1, Total>::run(base, stride);
    }
};

template<int Total>
struct StridedIssuer<Total, Total>
{
    static inline void run(const std::uint8_t *, std::uint64_t) {}
};

template<int Unroll>
__attribute__((noinline))
void run_strided(const std::uint8_t *data, const Case &test_case,
    std::uint64_t cache_line_bytes, std::uint64_t passes)
{
    const std::uint64_t phases = test_case.stride_bytes / cache_line_bytes;
    const std::uint64_t loads_per_phase =
        test_case.workset_bytes / test_case.stride_bytes;
    const std::uint64_t groups = loads_per_phase / Unroll;
    std::uint64_t delay_state = reinterpret_cast<std::uintptr_t>(data);
    for (std::uint64_t pass = 0; pass < passes; ++pass) {
        for (std::uint64_t phase = 0; phase < phases; ++phase) {
            const std::uint8_t *pointer = data + phase * cache_line_bytes;
            for (std::uint64_t group = 0; group < groups; ++group) {
                StridedIssuer<0, Unroll>::run(pointer, test_case.stride_bytes);
                pointer += test_case.stride_bytes * Unroll;
                serial_delay(static_cast<std::uint64_t>(
                    test_case.delay_iterations), delay_state);
            }
        }
    }
    asm volatile("" : : "r"(delay_state) : "memory");
}

void dispatch_strided(const std::uint8_t *data, const Case &test_case,
    std::uint64_t cache_line_bytes, std::uint64_t passes)
{
    switch (test_case.unroll) {
    case 1: run_strided<1>(data, test_case, cache_line_bytes, passes); break;
    case 2: run_strided<2>(data, test_case, cache_line_bytes, passes); break;
    case 4: run_strided<4>(data, test_case, cache_line_bytes, passes); break;
    case 8: run_strided<8>(data, test_case, cache_line_bytes, passes); break;
    case 16: run_strided<16>(data, test_case, cache_line_bytes, passes); break;
    default: std::abort();
    }
}

#define RANDOM_LOOP_BEGIN "1:\n\t"
#define RANDOM_LOAD(pointer)                                                 \
    "ldr %[" #pointer "], [%[" #pointer "]]\n\t"
#define RANDOM_LOOP_END                                                      \
    "subs %[count], %[count], #1\n\t"                                      \
    "b.ne 1b\n\t"

__attribute__((noinline)) void run_random_1(
    const std::array<std::uint8_t *, 16> &heads,
    std::uint64_t iterations, std::uint64_t passes)
{
    std::uintptr_t p0 = reinterpret_cast<std::uintptr_t>(heads[0]);
    for (std::uint64_t pass = 0; pass < passes; ++pass) {
        std::uint64_t count = iterations;
        asm volatile(
            RANDOM_LOOP_BEGIN
            RANDOM_LOAD(p0)
            RANDOM_LOOP_END
            : [p0] "+&r"(p0), [count] "+&r"(count)
            :
            : "cc", "memory");
    }
    asm volatile("" : : "r"(p0) : "memory");
}

__attribute__((noinline)) void run_random_2(
    const std::array<std::uint8_t *, 16> &heads,
    std::uint64_t iterations, std::uint64_t passes)
{
    std::uintptr_t p0 = reinterpret_cast<std::uintptr_t>(heads[0]);
    std::uintptr_t p1 = reinterpret_cast<std::uintptr_t>(heads[1]);
    for (std::uint64_t pass = 0; pass < passes; ++pass) {
        std::uint64_t count = iterations;
        asm volatile(
            RANDOM_LOOP_BEGIN
            RANDOM_LOAD(p0) RANDOM_LOAD(p1)
            RANDOM_LOOP_END
            : [p0] "+&r"(p0), [p1] "+&r"(p1),
              [count] "+&r"(count)
            :
            : "cc", "memory");
    }
    asm volatile("" : : "r"(p0), "r"(p1) : "memory");
}

__attribute__((noinline)) void run_random_4(
    const std::array<std::uint8_t *, 16> &heads,
    std::uint64_t iterations, std::uint64_t passes)
{
    std::uintptr_t p0 = reinterpret_cast<std::uintptr_t>(heads[0]);
    std::uintptr_t p1 = reinterpret_cast<std::uintptr_t>(heads[1]);
    std::uintptr_t p2 = reinterpret_cast<std::uintptr_t>(heads[2]);
    std::uintptr_t p3 = reinterpret_cast<std::uintptr_t>(heads[3]);
    for (std::uint64_t pass = 0; pass < passes; ++pass) {
        std::uint64_t count = iterations;
        asm volatile(
            RANDOM_LOOP_BEGIN
            RANDOM_LOAD(p0) RANDOM_LOAD(p1) RANDOM_LOAD(p2) RANDOM_LOAD(p3)
            RANDOM_LOOP_END
            : [p0] "+&r"(p0), [p1] "+&r"(p1),
              [p2] "+&r"(p2), [p3] "+&r"(p3),
              [count] "+&r"(count)
            :
            : "cc", "memory");
    }
    asm volatile("" : : "r"(p0), "r"(p1), "r"(p2), "r"(p3) : "memory");
}

__attribute__((noinline)) void run_random_8(
    const std::array<std::uint8_t *, 16> &heads,
    std::uint64_t iterations, std::uint64_t passes)
{
    std::uintptr_t p0 = reinterpret_cast<std::uintptr_t>(heads[0]);
    std::uintptr_t p1 = reinterpret_cast<std::uintptr_t>(heads[1]);
    std::uintptr_t p2 = reinterpret_cast<std::uintptr_t>(heads[2]);
    std::uintptr_t p3 = reinterpret_cast<std::uintptr_t>(heads[3]);
    std::uintptr_t p4 = reinterpret_cast<std::uintptr_t>(heads[4]);
    std::uintptr_t p5 = reinterpret_cast<std::uintptr_t>(heads[5]);
    std::uintptr_t p6 = reinterpret_cast<std::uintptr_t>(heads[6]);
    std::uintptr_t p7 = reinterpret_cast<std::uintptr_t>(heads[7]);
    for (std::uint64_t pass = 0; pass < passes; ++pass) {
        std::uint64_t count = iterations;
        asm volatile(
            RANDOM_LOOP_BEGIN
            RANDOM_LOAD(p0) RANDOM_LOAD(p1) RANDOM_LOAD(p2) RANDOM_LOAD(p3)
            RANDOM_LOAD(p4) RANDOM_LOAD(p5) RANDOM_LOAD(p6) RANDOM_LOAD(p7)
            RANDOM_LOOP_END
            : [p0] "+&r"(p0), [p1] "+&r"(p1),
              [p2] "+&r"(p2), [p3] "+&r"(p3),
              [p4] "+&r"(p4), [p5] "+&r"(p5),
              [p6] "+&r"(p6), [p7] "+&r"(p7),
              [count] "+&r"(count)
            :
            : "cc", "memory");
    }
    asm volatile("" : :
        "r"(p0), "r"(p1), "r"(p2), "r"(p3),
        "r"(p4), "r"(p5), "r"(p6), "r"(p7) : "memory");
}

__attribute__((noinline)) void run_random_16(
    const std::array<std::uint8_t *, 16> &heads,
    std::uint64_t iterations, std::uint64_t passes)
{
    // GCC counts every read/write operand twice and rejects an extended-asm
    // statement with 16 pointer outputs.  Use fixed scratch registers here.
    // Each chain is a cycle of exactly `iterations` nodes, so its final value
    // is intentionally not exported back to C++.
    asm volatile(
        "ldp x4,  x5,  [%[heads], #0]\n\t"
        "ldp x6,  x7,  [%[heads], #16]\n\t"
        "ldp x8,  x9,  [%[heads], #32]\n\t"
        "ldp x10, x11, [%[heads], #48]\n\t"
        "ldp x12, x13, [%[heads], #64]\n\t"
        "ldp x14, x15, [%[heads], #80]\n\t"
        "ldp x16, x17, [%[heads], #96]\n\t"
        "ldp x19, x22, [%[heads], #112]\n\t"
        "mov x20, %[passes]\n\t"
        "2:\n\t"
        "mov x21, %[iterations]\n\t"
        "1:\n\t"
        "ldr x4,  [x4]\n\t"
        "ldr x5,  [x5]\n\t"
        "ldr x6,  [x6]\n\t"
        "ldr x7,  [x7]\n\t"
        "ldr x8,  [x8]\n\t"
        "ldr x9,  [x9]\n\t"
        "ldr x10, [x10]\n\t"
        "ldr x11, [x11]\n\t"
        "ldr x12, [x12]\n\t"
        "ldr x13, [x13]\n\t"
        "ldr x14, [x14]\n\t"
        "ldr x15, [x15]\n\t"
        "ldr x16, [x16]\n\t"
        "ldr x17, [x17]\n\t"
        "ldr x19, [x19]\n\t"
        "ldr x22, [x22]\n\t"
        "subs x21, x21, #1\n\t"
        "b.ne 1b\n\t"
        "subs x20, x20, #1\n\t"
        "b.ne 2b\n\t"
        :
        : [heads] "r"(heads.data()), [iterations] "r"(iterations),
          [passes] "r"(passes)
        : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
          "x12", "x13", "x14", "x15", "x16", "x17", "x19", "x20",
          "x21", "x22", "cc", "memory");
}

#undef RANDOM_LOOP_BEGIN
#undef RANDOM_LOAD
#undef RANDOM_LOOP_END

void dispatch_random(const std::array<std::uint8_t *, 16> &heads,
    const Case &test_case, std::uint64_t passes)
{
    const std::uint64_t iterations = test_case.loads_per_pass /
        static_cast<std::uint64_t>(test_case.random_chains);
    switch (test_case.random_chains) {
    case 1: run_random_1(heads, iterations, passes); break;
    case 2: run_random_2(heads, iterations, passes); break;
    case 4: run_random_4(heads, iterations, passes); break;
    case 8: run_random_8(heads, iterations, passes); break;
    case 16: run_random_16(heads, iterations, passes); break;
    default: std::abort();
    }
}

class Barrier
{
public:
    explicit Barrier(std::size_t participants) :
        participants_(participants), arrived_(0), generation_(0) {}

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const std::size_t generation = generation_;
        if (++arrived_ == participants_) {
            arrived_ = 0;
            ++generation_;
            condition_.notify_all();
        } else {
            condition_.wait(lock,
                [&] { return generation != generation_; });
        }
    }

private:
    std::size_t participants_;
    std::size_t arrived_;
    std::size_t generation_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

enum class CommandKind
{
    InitializeRandom,
    Warmup,
    Measure,
};

struct Command
{
    CommandKind kind = CommandKind::Warmup;
    const Case *test_case = nullptr;
    std::uint64_t passes = 1;
};

struct SharedState
{
    explicit SharedState(std::size_t workers) :
        ready(workers + 1), start(workers + 1), finish(workers + 1),
        records(workers) {}

    Barrier ready;
    Barrier start;
    Barrier finish;
    Command command;
    bool stop = false;
    std::uint64_t cache_line_bytes = 64;
    std::vector<WorkerRecord> records;
};

struct Worker
{
    int cpu = 0;
    std::size_t index = 0;
    std::uint64_t allocation_bytes = 0;
    double fallback_frequency_hz = 0.0;
    SharedState *shared = nullptr;
    std::thread thread;
    std::uint8_t *data = nullptr;
    std::string error;
    std::vector<std::uint32_t> random_order;
    std::uint64_t random_nodes = 0;
    std::array<std::uint8_t *, 16> random_heads{};
};

void initialize_random(Worker &worker, const Case &test_case,
    std::uint64_t cache_line_bytes)
{
    const std::uint64_t nodes = test_case.loads_per_pass;
    if (nodes == 0 || nodes > std::numeric_limits<std::uint32_t>::max()) {
        worker.error = "random workset has an unsupported node count";
        return;
    }
    if (worker.random_nodes != nodes) {
        worker.random_order.resize(static_cast<std::size_t>(nodes));
        std::iota(worker.random_order.begin(), worker.random_order.end(), 0U);
        std::mt19937 generator(0xC0FFEEU +
            static_cast<std::uint32_t>(worker.index * 0x9E37U));
        std::shuffle(worker.random_order.begin(), worker.random_order.end(),
            generator);
        worker.random_nodes = nodes;
    }

    const std::uint64_t chains = test_case.random_chains;
    const std::uint64_t nodes_per_chain = nodes / chains;
    for (std::uint64_t chain = 0; chain < chains; ++chain) {
        const std::uint64_t begin = chain * nodes_per_chain;
        worker.random_heads[chain] = worker.data +
            static_cast<std::uint64_t>(worker.random_order[begin]) *
                cache_line_bytes;
        for (std::uint64_t offset = 0; offset < nodes_per_chain; ++offset) {
            const std::uint64_t current_index =
                worker.random_order[begin + offset];
            const std::uint64_t next_index = worker.random_order[
                begin + (offset + 1) % nodes_per_chain];
            std::uint8_t *current = worker.data +
                current_index * cache_line_bytes;
            std::uint8_t *next = worker.data + next_index * cache_line_bytes;
            std::memcpy(current, &next, sizeof(next));
        }
    }
}

void execute_case(Worker &worker, const Case &test_case,
    std::uint64_t passes)
{
    if (test_case.kernel == KernelKind::Dense) {
        dispatch_dense(worker.data, test_case, passes);
    } else if (test_case.kernel == KernelKind::Strided) {
        dispatch_strided(worker.data, test_case,
            worker.shared->cache_line_bytes, passes);
    } else {
        dispatch_random(worker.random_heads, test_case, passes);
    }
}

void worker_main(Worker *worker)
{
    if (!pin_current_thread(worker->cpu, worker->error)) goto ready;
    worker->data = static_cast<std::uint8_t *>(mmap(nullptr,
        static_cast<std::size_t>(worker->allocation_bytes),
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0));
    if (worker->data == MAP_FAILED) {
        worker->data = nullptr;
        worker->error = "mmap(" + format_bytes(worker->allocation_bytes) +
            "): " + std::strerror(errno);
        goto ready;
    }
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    (void)madvise(worker->data,
        static_cast<std::size_t>(worker->allocation_bytes), MADV_HUGEPAGE);
#endif
    std::memset(worker->data, 1,
        static_cast<std::size_t>(worker->allocation_bytes));

ready:
    worker->shared->ready.wait();

    CycleCounter counter;
    while (true) {
        worker->shared->start.wait();
        if (worker->shared->stop) break;
        const Command command = worker->shared->command;
        if (!worker->error.empty()) {
            // Keep participating in the barriers so the main thread can
            // terminate every worker cleanly after reporting the error.
        } else if (command.kind == CommandKind::InitializeRandom) {
            initialize_random(*worker, *command.test_case,
                worker->shared->cache_line_bytes);
        } else if (command.kind == CommandKind::Warmup) {
            execute_case(*worker, *command.test_case, command.passes);
        } else {
            WorkerRecord &record = worker->shared->records[worker->index];
            const bool counter_started = counter.start();
            clock_gettime(CLOCK_MONOTONIC_RAW, &record.start);
            execute_case(*worker, *command.test_case, command.passes);
            clock_gettime(CLOCK_MONOTONIC_RAW, &record.end);
            record.cycles = counter_started ? counter.stop() : 0;
        }
        worker->shared->finish.wait();
    }

    munmap(worker->data, static_cast<std::size_t>(worker->allocation_bytes));
    worker->data = nullptr;
}

class WorkerPool
{
public:
    WorkerPool(const Options &options, std::uint64_t allocation_bytes,
        std::uint64_t cache_line_bytes) :
        options_(options), shared_(options.cpus.size()),
        workers_(options.cpus.size())
    {
        shared_.cache_line_bytes = cache_line_bytes;
        for (std::size_t index = 0; index < workers_.size(); ++index) {
            Worker &worker = workers_[index];
            worker.cpu = options.cpus[index];
            worker.index = index;
            worker.allocation_bytes = allocation_bytes;
            worker.fallback_frequency_hz = options.frequency_hz > 0.0
                ? options.frequency_hz : read_cpu_frequency_hz(worker.cpu);
            worker.shared = &shared_;
            worker.thread = std::thread(worker_main, &worker);
        }
        shared_.ready.wait();
        for (const Worker &worker : workers_) {
            if (!worker.error.empty()) {
                error_ = "CPU " + std::to_string(worker.cpu) + ": " +
                    worker.error;
                break;
            }
        }
    }

    ~WorkerPool()
    {
        shared_.stop = true;
        shared_.start.wait();
        for (Worker &worker : workers_) {
            if (worker.thread.joinable()) worker.thread.join();
        }
    }

    bool good() const { return error_.empty(); }
    const std::string &error() const { return error_; }

    bool initialize_random_case(const Case &test_case)
    {
        if (!dispatch(CommandKind::InitializeRandom, test_case, 1)) return false;
        for (const Worker &worker : workers_) {
            if (!worker.error.empty()) {
                error_ = "CPU " + std::to_string(worker.cpu) + ": " +
                    worker.error;
                return false;
            }
        }
        return true;
    }

    bool warmup(const Case &test_case)
    {
        return dispatch(CommandKind::Warmup, test_case, 1);
    }

    Sample measure(const Case &test_case, std::uint64_t passes,
        std::string &cycle_source)
    {
        Sample sample;
        if (!dispatch(CommandKind::Measure, test_case, passes)) return sample;
        timespec first = shared_.records.front().start;
        timespec last = shared_.records.front().end;
        long double worker_seconds = 0.0;
        long double measured_cycles = 0.0;
        bool all_pmu = true;
        bool all_frequency = true;
        for (std::size_t index = 0; index < workers_.size(); ++index) {
            const WorkerRecord &record = shared_.records[index];
            if (earlier(record.start, first)) first = record.start;
            if (earlier(last, record.end)) last = record.end;
            const double seconds = elapsed_seconds(record.start, record.end);
            worker_seconds += seconds;
            if (record.cycles == 0) all_pmu = false;
            measured_cycles += record.cycles;
            if (workers_[index].fallback_frequency_hz <= 0.0)
                all_frequency = false;
        }
        sample.seconds = elapsed_seconds(first, last);
        if (!all_pmu) {
            measured_cycles = 0.0;
            if (all_frequency) {
                for (std::size_t index = 0; index < workers_.size(); ++index) {
                    measured_cycles += elapsed_seconds(
                        shared_.records[index].start,
                        shared_.records[index].end) *
                        workers_[index].fallback_frequency_hz;
                }
            }
        }
        cycle_source = all_pmu ? "perf-cycles" :
            all_frequency ? "frequency-estimate" : "unavailable";

        const long double worker_count = workers_.size();
        const long double total_loads =
            static_cast<long double>(test_case.loads_per_pass) * passes *
            worker_count;
        const long double useful_bytes =
            static_cast<long double>(test_case.useful_bytes_per_pass) * passes *
            worker_count;
        const long double line_bytes =
            static_cast<long double>(test_case.line_bytes_per_pass) * passes *
            worker_count;
        if (sample.seconds > 0.0) {
            sample.useful_gb_per_second = static_cast<double>(
                useful_bytes / sample.seconds / 1.0e9L);
            sample.line_gb_per_second = static_cast<double>(
                line_bytes / sample.seconds / 1.0e9L);
        }
        if (measured_cycles > 0.0)
            sample.load_ipc = static_cast<double>(total_loads / measured_cycles);
        if (worker_seconds > 0.0) {
            sample.load_gips_per_core = static_cast<double>(
                total_loads / worker_seconds / 1.0e9L);
        }
        return sample;
    }

private:
    bool dispatch(CommandKind kind, const Case &test_case,
        std::uint64_t passes)
    {
        if (!error_.empty()) return false;
        shared_.command.kind = kind;
        shared_.command.test_case = &test_case;
        shared_.command.passes = passes;
        if (kind == CommandKind::Measure) {
            for (WorkerRecord &record : shared_.records) record = WorkerRecord();
        }
        shared_.start.wait();
        shared_.finish.wait();
        return true;
    }

    Options options_;
    SharedState shared_;
    std::vector<Worker> workers_;
    std::string error_;
};

std::string case_key(const Case &test_case)
{
    std::ostringstream stream;
    // section/pattern are presentation metadata.  Reuse a measurement when
    // an access-pattern row has exactly the same executable configuration as
    // an envelope row.
    stream << test_case.level << '|' << static_cast<int>(test_case.kernel) << '|'
           << test_case.workset_bytes << '|' << test_case.stride_bytes << '|'
           << test_case.streams << '|' << test_case.unroll << '|'
           << test_case.delay_iterations << '|' << test_case.random_chains;
    return stream.str();
}

Case make_dense_case(const LevelSpec &level, std::uint64_t cache_line_bytes,
    const std::string &section, const std::string &pattern,
    int streams, int unroll, int delay_iterations)
{
    Case result;
    result.section = section;
    result.level = level.name;
    result.pattern = pattern;
    result.kernel = KernelKind::Dense;
    result.streams = streams;
    result.unroll = unroll;
    result.delay_iterations = delay_iterations;
    const std::uint64_t group_bytes = static_cast<std::uint64_t>(streams) *
        unroll * kVectorBytes;
    const std::uint64_t alignment = std::lcm(cache_line_bytes, group_bytes);
    result.workset_bytes = align_down(level.bytes_per_worker, alignment);
    result.loads_per_pass = result.workset_bytes / kVectorBytes;
    result.useful_bytes_per_pass = result.workset_bytes;
    result.line_bytes_per_pass = result.workset_bytes;
    return result;
}

int choose_unroll(std::uint64_t loads_per_phase)
{
    if (loads_per_phase >= 16) return 16;
    if (loads_per_phase >= 8) return 8;
    if (loads_per_phase >= 4) return 4;
    if (loads_per_phase >= 2) return 2;
    return 1;
}

Case make_strided_case(const LevelSpec &level,
    std::uint64_t cache_line_bytes, const std::string &pattern,
    std::uint64_t stride_bytes)
{
    Case result;
    result.section = "patterns";
    result.level = level.name;
    result.pattern = pattern;
    result.kernel = KernelKind::Strided;
    result.stride_bytes = stride_bytes;
    result.workset_bytes = align_down(level.bytes_per_worker, stride_bytes);
    const std::uint64_t loads_per_phase = result.workset_bytes / stride_bytes;
    result.unroll = choose_unroll(loads_per_phase);
    result.workset_bytes = align_down(result.workset_bytes,
        stride_bytes * static_cast<std::uint64_t>(result.unroll));
    result.loads_per_pass = result.workset_bytes / cache_line_bytes;
    result.useful_bytes_per_pass = result.loads_per_pass * kVectorBytes;
    result.line_bytes_per_pass = result.workset_bytes;
    return result;
}

Case make_random_case(const LevelSpec &level,
    std::uint64_t cache_line_bytes, int chains)
{
    Case result;
    result.section = "patterns";
    result.level = level.name;
    result.pattern = chains == 1 ? "pointer-chain" :
        "random-" + std::to_string(chains) + "-chains";
    result.kernel = KernelKind::Random;
    result.random_chains = chains;
    std::uint64_t nodes = greatest_power_of_two(
        level.bytes_per_worker / cache_line_bytes);
    while (nodes >= static_cast<std::uint64_t>(chains) &&
        nodes % static_cast<std::uint64_t>(chains) != 0) {
        nodes >>= 1;
    }
    result.workset_bytes = nodes * cache_line_bytes;
    result.loads_per_pass = nodes;
    result.useful_bytes_per_load = sizeof(std::uintptr_t);
    result.useful_bytes_per_pass = nodes * sizeof(std::uintptr_t);
    result.line_bytes_per_pass = result.workset_bytes;
    return result;
}

std::vector<Case> make_envelope_cases(const LevelSpec &level,
    std::uint64_t cache_line_bytes)
{
    std::vector<Case> result;
    std::set<std::string> seen;
    const int unrolls[] = {1, 2, 4, 8, 16};
    for (const int unroll : unrolls) {
        Case test_case = make_dense_case(level, cache_line_bytes,
            "envelope", "dense", 1, unroll, 0);
        if (test_case.workset_bytes > 0 && seen.insert(case_key(test_case)).second)
            result.push_back(test_case);
    }
    const int streams[] = {1, 2, 4, 8};
    for (const int stream_count : streams) {
        Case test_case = make_dense_case(level, cache_line_bytes,
            "envelope", "dense", stream_count, 4, 0);
        if (test_case.workset_bytes > 0 && seen.insert(case_key(test_case)).second)
            result.push_back(test_case);
    }
    const int delays[] = {
        0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
    };
    for (const int delay : delays) {
        Case test_case = make_dense_case(level, cache_line_bytes,
            "envelope", "dense", 4, 4, delay);
        if (test_case.workset_bytes > 0 && seen.insert(case_key(test_case)).second)
            result.push_back(test_case);
    }
    return result;
}

std::vector<Case> make_pattern_cases(const Options &options,
    const LevelSpec &level, std::uint64_t cache_line_bytes)
{
    std::vector<Case> result;
    if (options.patterns.count(PatternKind::Dense) != 0) {
        result.push_back(make_dense_case(level, cache_line_bytes,
            "patterns", "dense", 4, 4, 0));
    }
    if (options.patterns.count(PatternKind::Line) != 0) {
        result.push_back(make_strided_case(level, cache_line_bytes,
            "one-vector-per-line", cache_line_bytes));
    }
    if (options.patterns.count(PatternKind::Stride2) != 0) {
        result.push_back(make_strided_case(level, cache_line_bytes,
            "stride-2cl", cache_line_bytes * 2));
    }
    if (options.patterns.count(PatternKind::Stride4) != 0) {
        result.push_back(make_strided_case(level, cache_line_bytes,
            "stride-4cl", cache_line_bytes * 4));
    }
    if (options.patterns.count(PatternKind::Page) != 0 &&
        4096 % cache_line_bytes == 0 && level.bytes_per_worker >= 4096) {
        result.push_back(make_strided_case(level, cache_line_bytes,
            "stride-4KiB", 4096));
    }
    if (options.patterns.count(PatternKind::Multi) != 0) {
        const int streams[] = {1, 2, 4, 8};
        for (const int stream_count : streams) {
            result.push_back(make_dense_case(level, cache_line_bytes,
                "patterns", "dense-" + std::to_string(stream_count) +
                    "-streams", stream_count, 4, 0));
        }
    }
    if (options.patterns.count(PatternKind::Random) != 0) {
        const int chains[] = {1, 2, 4, 8, 16};
        for (const int chain_count : chains) {
            Case test_case = make_random_case(level, cache_line_bytes,
                chain_count);
            if (test_case.loads_per_pass >=
                static_cast<std::uint64_t>(chain_count)) {
                result.push_back(test_case);
            }
        }
    }
    return result;
}

Result measure_case(WorkerPool &pool, const Options &options,
    const Case &test_case)
{
    Result result;
    result.test_case = test_case;
    if (test_case.kernel == KernelKind::Random &&
        !pool.initialize_random_case(test_case)) {
        return result;
    }
    if (!pool.warmup(test_case)) return result;

    std::string probe_source;
    const Sample probe = pool.measure(test_case, 1, probe_source);
    if (probe.seconds <= 0.0) return result;
    const double target_seconds = options.target_milliseconds * 1.0e-3;
    long double calibrated = std::ceil(target_seconds / probe.seconds);
    if (calibrated < 1.0L) calibrated = 1.0L;
    if (calibrated > static_cast<long double>(kMaximumPasses))
        calibrated = static_cast<long double>(kMaximumPasses);
    result.passes = static_cast<std::uint64_t>(calibrated);

    std::vector<Sample> samples;
    samples.reserve(options.repetitions);
    for (int repetition = 0; repetition < options.repetitions; ++repetition) {
        std::string source;
        const Sample sample = pool.measure(test_case, result.passes, source);
        if (sample.seconds <= 0.0 || !std::isfinite(sample.line_gb_per_second))
            continue;
        result.cycle_source = source;
        samples.push_back(sample);
    }
    if (samples.empty()) return result;
    std::sort(samples.begin(), samples.end(),
        [](const Sample &left, const Sample &right) {
            return left.line_gb_per_second < right.line_gb_per_second;
        });
    result.median = samples[samples.size() / 2];
    result.minimum_line_gb_per_second = samples.front().line_gb_per_second;
    result.maximum_line_gb_per_second = samples.back().line_gb_per_second;
    return result;
}

std::string decimal(double value, int precision = 3)
{
    if (value <= 0.0 || !std::isfinite(value)) return "-";
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

void print_envelope(const std::vector<Result> &results)
{
    double peak = 0.0;
    for (const Result &result : results)
        peak = std::max(peak, result.median.line_gb_per_second);

    std::cout << "\n[load_envelope]\n"
              << "Level,Streams,Unroll/stream,Delay iters/group,"
              << "Workset/worker,Median GB/s,Load IPC/core,"
              << "Load Ginst/s/core,% peak,Min GB/s,Max GB/s,Passes,Cycle source\n";
    for (const Result &result : results) {
        const double percent = peak > 0.0
            ? result.median.line_gb_per_second / peak * 100.0 : 0.0;
        const Case &test_case = result.test_case;
        std::cout << test_case.level << ',' << test_case.streams << ','
                  << test_case.unroll << ',' << test_case.delay_iterations << ','
                  << format_bytes(test_case.workset_bytes) << ','
                  << decimal(result.median.line_gb_per_second) << ','
                  << decimal(result.median.load_ipc) << ','
                  << decimal(result.median.load_gips_per_core) << ','
                  << decimal(percent, 1) << ','
                  << decimal(result.minimum_line_gb_per_second) << ','
                  << decimal(result.maximum_line_gb_per_second) << ','
                  << result.passes << ',' << result.cycle_source << '\n';
    }

    const Result *knee = nullptr;
    for (const Result &result : results) {
        if (result.median.line_gb_per_second < peak * 0.95) continue;
        if (knee == nullptr ||
            (result.median.load_ipc > 0.0 &&
             (knee->median.load_ipc <= 0.0 ||
              result.median.load_ipc < knee->median.load_ipc)) ||
            (result.median.load_ipc <= 0.0 && knee->median.load_ipc <= 0.0 &&
             result.median.load_gips_per_core <
                knee->median.load_gips_per_core)) {
            knee = &result;
        }
    }
    if (knee != nullptr) {
        std::cout << "\n[load_envelope_summary]\n"
                  << "Level,Peak GB/s,LIPC95/core,Load Ginst/s/core95,"
                  << "Streams,Unroll/stream,Delay iters/group,Workset/worker\n"
                  << knee->test_case.level << ',' << decimal(peak) << ','
                  << decimal(knee->median.load_ipc) << ','
                  << decimal(knee->median.load_gips_per_core) << ','
                  << knee->test_case.streams << ',' << knee->test_case.unroll
                  << ',' << knee->test_case.delay_iterations << ','
                  << format_bytes(knee->test_case.workset_bytes) << '\n';
    }
}

void print_patterns(const std::vector<Result> &results, double dense_peak,
    std::uint64_t cache_line_bytes)
{
    std::cout << "\n[access_patterns]\n"
              << "Level,Pattern,Instruction,Bytes/load,Streams/Chains,Unroll,"
              << "Stride,Workset/worker,Useful GB/s,CL-equivalent GB/s,"
              << "Load IPC/core,Load Ginst/s/core,% dense useful,"
              << "% dense CL-equivalent,Min CL GB/s,"
              << "Max CL GB/s,Passes,Cycle source\n";
    for (const Result &result : results) {
        const Case &test_case = result.test_case;
        const int concurrency = test_case.kernel == KernelKind::Random
            ? test_case.random_chains : test_case.streams;
        const std::string instruction = test_case.kernel == KernelKind::Random
            ? "ldr-x" : "neon-ld1h";
        const double useful_percent = dense_peak > 0.0
            ? result.median.useful_gb_per_second / dense_peak * 100.0 : 0.0;
        const double line_percent = dense_peak > 0.0
            ? result.median.line_gb_per_second / dense_peak * 100.0 : 0.0;
        std::cout << test_case.level << ',' << test_case.pattern << ','
                  << instruction << ',' << test_case.useful_bytes_per_load
                  << ',' << concurrency << ',' << test_case.unroll << ',';
        if (test_case.kernel == KernelKind::Random)
            std::cout << "random";
        else
            std::cout << test_case.stride_bytes << " B";
        std::cout << ',' << format_bytes(test_case.workset_bytes) << ','
                  << decimal(result.median.useful_gb_per_second) << ','
                  << decimal(result.median.line_gb_per_second) << ','
                  << decimal(result.median.load_ipc) << ','
                  << decimal(result.median.load_gips_per_core) << ','
                  << decimal(useful_percent, 1) << ','
                  << decimal(line_percent, 1) << ','
                  << decimal(result.minimum_line_gb_per_second) << ','
                  << decimal(result.maximum_line_gb_per_second) << ','
                  << result.passes << ',' << result.cycle_source << '\n';
    }
    std::cout << "# CL-equivalent normalizes each distinct line to "
              << cache_line_bytes << " B; it is a service-rate normalization, "
              << "not a PMU measurement of physical traffic.\n";
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 1;
    }

    const Topology topology = detect_topology(options.cpus.front());
    if (topology.line_bytes < kVectorBytes ||
        topology.line_bytes % kVectorBytes != 0 ||
        4096 % topology.line_bytes != 0) {
        std::cerr << "Unsupported cache-line size: " << topology.line_bytes
                  << " bytes.\n";
        return 1;
    }
    const std::vector<LevelSpec> levels = select_level_specs(options, topology);
    if (levels.empty()) {
        std::cerr << "No requested cache/memory level has a valid workset.\n";
        return 1;
    }
    std::uint64_t maximum_workset = 0;
    for (const LevelSpec &level : levels)
        maximum_workset = std::max(maximum_workset, level.bytes_per_worker);
    if (maximum_workset == 0 ||
        maximum_workset > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        std::cerr << "Selected workset is too large for this build.\n";
        return 1;
    }

    std::cout << "# AArch64 load saturation: " << options.cpus.size()
              << " worker(s); canonical load=neon ld1 {v?.8h}, 16 B/load\n"
              << "# Cache line=" << topology.line_bytes << " B from "
              << topology.line_source << "\n"
              << "# L1=" << format_bytes(topology.l1.bytes) << " ("
              << topology.l1.source << "), L2="
              << format_bytes(topology.l2.bytes) << " (" << topology.l2.source
              << "), L3="
              << (topology.l3.bytes > 0 ? format_bytes(topology.l3.bytes) :
                  std::string("unavailable")) << "\n";
#if defined(__APPLE__)
    if (options.cpus.size() > 1) {
        std::cout << "# macOS does not expose exact CPU pinning; --cpus controls "
                  << "worker count only.\n";
    }
#endif
    for (const LevelSpec &level : levels) {
        std::cout << "# " << level.name << " workset: "
                  << format_bytes(level.bytes_per_worker) << "/worker; "
                  << level.source << "\n";
    }

    WorkerPool pool(options, maximum_workset, topology.line_bytes);
    if (!pool.good()) {
        std::cerr << "Worker initialization failed: " << pool.error() << '\n';
        return 1;
    }

    std::map<std::string, Result> measured;
    for (const LevelSpec &level : levels) {
        std::vector<Result> envelope_results;
        double dense_peak = 0.0;
        if (options.run_envelope) {
            for (const Case &test_case :
                    make_envelope_cases(level, topology.line_bytes)) {
                const std::string key = case_key(test_case);
                Result result = measure_case(pool, options, test_case);
                if (result.median.seconds <= 0.0) {
                    std::cerr << "Measurement failed for " << level.name
                              << " envelope case.\n";
                    return 1;
                }
                measured[key] = result;
                dense_peak = std::max(dense_peak,
                    result.median.line_gb_per_second);
                envelope_results.push_back(result);
            }
            print_envelope(envelope_results);
        }

        if (options.run_patterns) {
            std::vector<Result> pattern_results;
            for (const Case &test_case :
                    make_pattern_cases(options, level, topology.line_bytes)) {
                const std::string key = case_key(test_case);
                const auto existing = measured.find(key);
                Result result;
                if (existing != measured.end()) {
                    result = existing->second;
                    result.test_case.pattern = test_case.pattern;
                    result.test_case.section = "patterns";
                } else {
                    result = measure_case(pool, options, test_case);
                    if (result.median.seconds <= 0.0) {
                        std::cerr << "Measurement failed for " << level.name
                                  << " pattern " << test_case.pattern << ".\n";
                        return 1;
                    }
                    measured[key] = result;
                }
                if (test_case.kernel == KernelKind::Dense) {
                    dense_peak = std::max(dense_peak,
                        result.median.line_gb_per_second);
                }
                pattern_results.push_back(result);
            }
            print_patterns(pattern_results, dense_peak, topology.line_bytes);
        }
    }
    return 0;
}
