// SME BFMOPA + LD1H load/compute-ratio grid microbenchmark (no stores).
//
// Build on an AArch64 host with an SME-aware compiler:
//   clang++ -O3 -std=c++17 -march=armv9.2-a+sme \
//       tmp/sme_bfmopa_ldst_ipc.cpp -o /tmp/sme_bfmopa_ldst_ipc
//   g++ -O3 -std=c++17 -march=armv9.2-a+sme \
//       tmp/sme_bfmopa_ldst_ipc.cpp -o /tmp/sme_bfmopa_ldst_ipc
//
// Examples:
//   /tmp/sme_bfmopa_ldst_ipc --ratio 1.6875 --iterations 1000000 --trials 9
//   /tmp/sme_bfmopa_ldst_ipc --grid --iterations 1000000 --trials 9
//   /tmp/sme_bfmopa_ldst_ipc --grid --cpu 0            # Linux only
//
// Each static loop body has exactly 16 BFMOPA instructions and no ST1W.
// The supported LD1H:BFMOPA grid is:
//   0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.6875, 2, 2.5, 3, 4.
//
// A primary LD1H writes z0, which is the source of the following BFMOPA.
// Extra LD1H instructions write z3..z7 and are distributed through the body;
// this makes them independent load issue work without changing the BFMOPA
// source.  Input is an aligned, cache-hot BF16 buffer.  Thus the benchmark
// focuses on load/compute instruction issue rather than memory bandwidth.
//
// Linux reports PMU IPC through perf_event_open when permissions allow it.
// macOS reports throughput and can optionally print a clearly labelled IPC
// estimate when --core-ghz is supplied; it does not use private PMU APIs.

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/perf_event.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif

#if defined(__aarch64__) && defined(__ARM_FEATURE_SME)
#define SME_BENCH_COMPILED 1
#else
#define SME_BENCH_COMPILED 0
#endif

namespace {

enum class RatioPattern {
    kL04,
    kL08,
    kL12,
    kL16,
    kL20,
    kL24,
    kL27,
    kL32,
    kL40,
    kL48,
    kL64,
};

struct RatioSpec {
    RatioPattern pattern;
    std::uint64_t ld1h_per_iteration;
    const char *ratio_text;
};

constexpr std::uint64_t kBfmopaPerIteration = 16;
constexpr RatioSpec kRatioSpecs[] = {
    {RatioPattern::kL04, 4, "0.25"},
    {RatioPattern::kL08, 8, "0.5"},
    {RatioPattern::kL12, 12, "0.75"},
    {RatioPattern::kL16, 16, "1"},
    {RatioPattern::kL20, 20, "1.25"},
    {RatioPattern::kL24, 24, "1.5"},
    {RatioPattern::kL27, 27, "1.6875"},
    {RatioPattern::kL32, 32, "2"},
    {RatioPattern::kL40, 40, "2.5"},
    {RatioPattern::kL48, 48, "3"},
    {RatioPattern::kL64, 64, "4"},
};
#if SME_BENCH_COMPILED
constexpr std::size_t kRatioSpecCount = sizeof(kRatioSpecs) / sizeof(kRatioSpecs[0]);
#endif
constexpr std::uint64_t kMaximumTargetInstructionsPerIteration =
    kBfmopaPerIteration + 64;
constexpr std::size_t kInputElements = 2048;

struct Options {
    std::uint64_t iterations = 1000000;
    unsigned int trials = 9;
    int cpu = -1;
    double core_ghz = 0.0;
    RatioPattern pattern = RatioPattern::kL16;
    bool grid = false;
};

struct Buffers {
    alignas(256) std::uint16_t input[kInputElements];
};

void print_usage(const char *program)
{
    std::cout
        << "Usage: " << program
        << " [--ratio R | --grid] [--iterations N] [--trials N] [--cpu N]"
        << " [--core-ghz GHz]\n\n"
        << "  --ratio R       LD1H:BFMOPA ratio: 0.25, 0.5, 0.75, 1, 1.25, 1.5,\n"
        << "                    1.6875, 2, 2.5, 3, or 4 (default: 1).\n"
        << "  --grid          Run every supported LD1H:BFMOPA ratio; no ST1W is emitted.\n"
        << "  --iterations N  Outer-loop iterations per ratio (default: 1000000).\n"
        << "  --trials N      Samples per ratio; report the best one (default: 9).\n"
        << "  --cpu N         Pin this thread to Linux CPU N. Unsupported on macOS.\n"
        << "  --core-ghz GHz  Estimate target IPC from wall time if no PMU IPC is available.\n";
}

bool parse_positive_u64(const char *text, std::uint64_t *value)
{
    if (text == nullptr || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0) {
        return false;
    }
    *value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_nonnegative_int(const char *text, int *value)
{
    if (text == nullptr || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool parse_positive_double(const char *text, double *value)
{
    if (text == nullptr || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0.0 ||
        !std::isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

bool parse_ratio(const char *text, RatioPattern *pattern)
{
    for (const RatioSpec &spec : kRatioSpecs) {
        if (std::strcmp(text, spec.ratio_text) == 0) {
            *pattern = spec.pattern;
            return true;
        }
    }
    return false;
}

bool parse_options(int argc, char **argv, Options *options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (argument == "--grid") {
            options->grid = true;
            continue;
        }
        if (index + 1 == argc) {
            std::cerr << "Missing value after " << argument << "\n";
            return false;
        }

        const char *value = argv[++index];
        if (argument == "--ratio") {
            if (!parse_ratio(value, &options->pattern)) {
                std::cerr << "Unsupported --ratio: " << value << "\n";
                return false;
            }
        } else if (argument == "--iterations") {
            if (!parse_positive_u64(value, &options->iterations)) {
                std::cerr << "--iterations must be a positive integer\n";
                return false;
            }
        } else if (argument == "--trials") {
            std::uint64_t parsed = 0;
            if (!parse_positive_u64(value, &parsed) || parsed > 1000) {
                std::cerr << "--trials must be an integer in [1, 1000]\n";
                return false;
            }
            options->trials = static_cast<unsigned int>(parsed);
        } else if (argument == "--cpu") {
            if (!parse_nonnegative_int(value, &options->cpu)) {
                std::cerr << "--cpu must be a non-negative integer\n";
                return false;
            }
        } else if (argument == "--core-ghz") {
            if (!parse_positive_double(value, &options->core_ghz)) {
                std::cerr << "--core-ghz must be a positive finite number\n";
                return false;
            }
        } else {
            std::cerr << "Unknown option: " << argument << "\n";
            return false;
        }
    }

    if (options->iterations >
        std::numeric_limits<std::uint64_t>::max() / kMaximumTargetInstructionsPerIteration) {
        std::cerr << "--iterations is too large\n";
        return false;
    }
    return true;
}

#if defined(__linux__) && SME_BENCH_COMPILED
bool pin_to_cpu(int cpu)
{
    if (cpu < 0) {
        return true;
    }
    if (cpu >= CPU_SETSIZE) {
        std::cerr << "Warning: CPU " << cpu << " is outside CPU_SETSIZE="
                  << CPU_SETSIZE << "\n";
        return false;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        std::cerr << "Warning: sched_setaffinity(" << cpu << ") failed: "
                  << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}
#endif

#if SME_BENCH_COMPILED

const RatioSpec &spec_for(RatioPattern pattern)
{
    for (const RatioSpec &spec : kRatioSpecs) {
        if (spec.pattern == pattern) {
            return spec;
        }
    }
    return kRatioSpecs[0];
}

// SMSTART/SMSTOP can alter normal vector state.  Preserve the AAPCS64
// callee-saved low halves before entering streaming mode, then restore them on
// exit.  z0, z1, z3..z7 and p0..p2 are caller-saved registers.
#define SME_KERNEL_PROLOGUE                                                       \
    "stp d8, d9, [sp, #-64]!\n"                                                  \
    "stp d10, d11, [sp, #16]\n"                                                 \
    "stp d12, d13, [sp, #32]\n"                                                 \
    "stp d14, d15, [sp, #48]\n"                                                 \
    "smstart\n"                                                                 \
    "ptrue p0.s\n"                                                               \
    "ptrue p1.s\n"                                                               \
    "ptrue p2.h\n"                                                               \
    "ld1h { z1.h }, p2/z, [%[input]]\n"                                         \
    "ld1h { z0.h }, p2/z, [%[input]]\n"

#define SME_KERNEL_EPILOGUE                                                       \
    "subs %x[repetitions], %x[repetitions], #1\n"                               \
    "b.ne 1b\n"                                                                  \
    "smstop\n"                                                                   \
    "ldp d8, d9, [sp, #0]\n"                                                    \
    "ldp d10, d11, [sp, #16]\n"                                                 \
    "ldp d12, d13, [sp, #32]\n"                                                 \
    "ldp d14, d15, [sp, #48]\n"                                                 \
    "add sp, sp, #64\n"

#define SME_C(ZA_INDEX)                                                           \
    "bfmopa za" #ZA_INDEX ".s, p0/m, p1/m, z0.h, z1.h\n"

#define SME_L0                                                                    \
    "ld1h { z0.h }, p2/z, [%[input]]\n"

#define SME_LX(Z_INDEX)                                                          \
    "ld1h { z" #Z_INDEX ".h }, p2/z, [%[input]]\n"

#define SME_C_L(ZA_INDEX)                                                        \
    SME_C(ZA_INDEX) SME_L0

#define SME_C_LX(ZA_INDEX, EXTRA_0)                                              \
    SME_C(ZA_INDEX) SME_L0 SME_LX(EXTRA_0)

#define SME_C_L2X(ZA_INDEX, EXTRA_0, EXTRA_1)                                    \
    SME_C(ZA_INDEX) SME_L0 SME_LX(EXTRA_0) SME_LX(EXTRA_1)

#define SME_C_L3X(ZA_INDEX, EXTRA_0, EXTRA_1, EXTRA_2)                           \
    SME_C(ZA_INDEX) SME_L0 SME_LX(EXTRA_0) SME_LX(EXTRA_1) SME_LX(EXTRA_2)

#define SME_DEFINE_KERNEL(NAME, BODY)                                            \
    __attribute__((noinline)) void NAME(std::uint64_t repetitions,              \
                                        const std::uint16_t *input)              \
    {                                                                              \
        asm volatile(                                                              \
            SME_KERNEL_PROLOGUE                                                   \
            "1:\n"                                                                \
            BODY                                                                   \
            SME_KERNEL_EPILOGUE                                                   \
            : [repetitions] "+&r"(repetitions)                                   \
            : [input] "r"(input)                                                 \
            : "cc", "memory", "p0", "p1", "p2", "z0", "z1", "z3", "z4",   \
              "z5", "z6", "z7");                                               \
    }

// 4 / 16 = 0.25 loads per compute: reload z0 after every fourth BFMOPA.
SME_DEFINE_KERNEL(run_sme_l04,
    SME_C(0) SME_C(1) SME_C(2) SME_C_L(3)
    SME_C(0) SME_C(1) SME_C(2) SME_C_L(3)
    SME_C(0) SME_C(1) SME_C(2) SME_C_L(3)
    SME_C(0) SME_C(1) SME_C(2) SME_C_L(3))

// 8 / 16 = 0.5 loads per compute.
SME_DEFINE_KERNEL(run_sme_l08,
    SME_C(0) SME_C_L(1) SME_C(2) SME_C_L(3)
    SME_C(0) SME_C_L(1) SME_C(2) SME_C_L(3)
    SME_C(0) SME_C_L(1) SME_C(2) SME_C_L(3)
    SME_C(0) SME_C_L(1) SME_C(2) SME_C_L(3))

// 12 / 16 = 0.75 loads per compute.
SME_DEFINE_KERNEL(run_sme_l12,
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C(3)
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C(3)
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C(3)
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C(3))

// 16 / 16 = 1 load per compute.
SME_DEFINE_KERNEL(run_sme_l16,
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C_L(3)
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C_L(3)
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C_L(3)
    SME_C_L(0) SME_C_L(1) SME_C_L(2) SME_C_L(3))

// 20 / 16 = 1.25 loads per compute.
SME_DEFINE_KERNEL(run_sme_l20,
    SME_C_LX(0, 3) SME_C_L(1) SME_C_L(2) SME_C_L(3)
    SME_C_LX(0, 4) SME_C_L(1) SME_C_L(2) SME_C_L(3)
    SME_C_LX(0, 5) SME_C_L(1) SME_C_L(2) SME_C_L(3)
    SME_C_LX(0, 6) SME_C_L(1) SME_C_L(2) SME_C_L(3))

// 24 / 16 = 1.5 loads per compute.
SME_DEFINE_KERNEL(run_sme_l24,
    SME_C_LX(0, 3) SME_C_LX(1, 4) SME_C_L(2) SME_C_L(3)
    SME_C_LX(0, 5) SME_C_LX(1, 6) SME_C_L(2) SME_C_L(3)
    SME_C_LX(0, 7) SME_C_LX(1, 3) SME_C_L(2) SME_C_L(3)
    SME_C_LX(0, 4) SME_C_LX(1, 5) SME_C_L(2) SME_C_L(3))

// 27 / 16 = 1.6875 loads per compute.  The 11 extra loads are spread through
// the 16 compute steps rather than emitted as one contiguous burst.
SME_DEFINE_KERNEL(run_sme_l27,
    SME_C_L(0)     SME_C_LX(1, 3) SME_C_LX(2, 4) SME_C_L(3)
    SME_C_LX(0, 5) SME_C_LX(1, 6) SME_C_L(2)     SME_C_LX(3, 7)
    SME_C_LX(0, 3) SME_C_L(1)     SME_C_LX(2, 4) SME_C_LX(3, 5)
    SME_C_L(0)     SME_C_LX(1, 6) SME_C_LX(2, 7) SME_C_LX(3, 3))

// 32 / 16 = 2 loads per compute.
SME_DEFINE_KERNEL(run_sme_l32,
    SME_C_LX(0, 3) SME_C_LX(1, 4) SME_C_LX(2, 5) SME_C_LX(3, 6)
    SME_C_LX(0, 7) SME_C_LX(1, 3) SME_C_LX(2, 4) SME_C_LX(3, 5)
    SME_C_LX(0, 6) SME_C_LX(1, 7) SME_C_LX(2, 3) SME_C_LX(3, 4)
    SME_C_LX(0, 5) SME_C_LX(1, 6) SME_C_LX(2, 7) SME_C_LX(3, 3))

// 40 / 16 = 2.5 loads per compute.
SME_DEFINE_KERNEL(run_sme_l40,
    SME_C_LX(0, 3) SME_C_L2X(1, 4, 5)
    SME_C_LX(2, 6) SME_C_L2X(3, 7, 3)
    SME_C_LX(0, 4) SME_C_L2X(1, 5, 6)
    SME_C_LX(2, 7) SME_C_L2X(3, 3, 4)
    SME_C_LX(0, 5) SME_C_L2X(1, 6, 7)
    SME_C_LX(2, 3) SME_C_L2X(3, 4, 5)
    SME_C_LX(0, 6) SME_C_L2X(1, 7, 3)
    SME_C_LX(2, 4) SME_C_L2X(3, 5, 6))

// 48 / 16 = 3 loads per compute.
SME_DEFINE_KERNEL(run_sme_l48,
    SME_C_L2X(0, 3, 4) SME_C_L2X(1, 5, 6) SME_C_L2X(2, 7, 3) SME_C_L2X(3, 4, 5)
    SME_C_L2X(0, 6, 7) SME_C_L2X(1, 3, 4) SME_C_L2X(2, 5, 6) SME_C_L2X(3, 7, 3)
    SME_C_L2X(0, 4, 5) SME_C_L2X(1, 6, 7) SME_C_L2X(2, 3, 4) SME_C_L2X(3, 5, 6)
    SME_C_L2X(0, 7, 3) SME_C_L2X(1, 4, 5) SME_C_L2X(2, 6, 7) SME_C_L2X(3, 3, 4))

// 64 / 16 = 4 loads per compute.
SME_DEFINE_KERNEL(run_sme_l64,
    SME_C_L3X(0, 3, 4, 5) SME_C_L3X(1, 6, 7, 3)
    SME_C_L3X(2, 4, 5, 6) SME_C_L3X(3, 7, 3, 4)
    SME_C_L3X(0, 5, 6, 7) SME_C_L3X(1, 3, 4, 5)
    SME_C_L3X(2, 6, 7, 3) SME_C_L3X(3, 4, 5, 6)
    SME_C_L3X(0, 7, 3, 4) SME_C_L3X(1, 5, 6, 7)
    SME_C_L3X(2, 3, 4, 5) SME_C_L3X(3, 6, 7, 3)
    SME_C_L3X(0, 4, 5, 6) SME_C_L3X(1, 7, 3, 4)
    SME_C_L3X(2, 5, 6, 7) SME_C_L3X(3, 3, 4, 5))

#undef SME_DEFINE_KERNEL
#undef SME_C_L3X
#undef SME_C_L2X
#undef SME_C_LX
#undef SME_C_L
#undef SME_LX
#undef SME_L0
#undef SME_C
#undef SME_KERNEL_EPILOGUE
#undef SME_KERNEL_PROLOGUE

using SmeKernel = void (*)(std::uint64_t, const std::uint16_t *);

SmeKernel kernel_for(RatioPattern pattern)
{
    switch (pattern) {
    case RatioPattern::kL04: return run_sme_l04;
    case RatioPattern::kL08: return run_sme_l08;
    case RatioPattern::kL12: return run_sme_l12;
    case RatioPattern::kL16: return run_sme_l16;
    case RatioPattern::kL20: return run_sme_l20;
    case RatioPattern::kL24: return run_sme_l24;
    case RatioPattern::kL27: return run_sme_l27;
    case RatioPattern::kL32: return run_sme_l32;
    case RatioPattern::kL40: return run_sme_l40;
    case RatioPattern::kL48: return run_sme_l48;
    case RatioPattern::kL64: return run_sme_l64;
    }
    return run_sme_l16;
}

void child_fault_handler(int signal_number)
{
    _exit(128 + signal_number);
}

bool run_sme_probe(SmeKernel kernel)
{
    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "fork() failed while probing SME: " << std::strerror(errno) << "\n";
        return false;
    }
    if (child == 0) {
        signal(SIGILL, child_fault_handler);
        signal(SIGBUS, child_fault_handler);
        signal(SIGSEGV, child_fault_handler);

        Buffers buffers{};
        for (std::size_t index = 0; index < kInputElements; ++index) {
            buffers.input[index] = 0x3f80;  // BF16 representation of 1.0.
        }
        kernel(1, buffers.input);
        _exit(0);
    }

    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            std::cerr << "waitpid() failed while probing SME: " << std::strerror(errno) << "\n";
            return false;
        }
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }
    if (WIFSIGNALED(status)) {
        std::cerr << "SME probe terminated by signal " << WTERMSIG(status) << "\n";
    } else if (WIFEXITED(status)) {
        std::cerr << "SME probe exited with status " << WEXITSTATUS(status) << "\n";
    }
    return false;
}

#endif  // SME_BENCH_COMPILED

#if defined(__linux__) && SME_BENCH_COMPILED

struct CounterReading {
    std::uint64_t cycles = 0;
    std::uint64_t instructions = 0;
};

class LinuxPerfGroup {
public:
    LinuxPerfGroup() = default;
    LinuxPerfGroup(const LinuxPerfGroup &) = delete;
    LinuxPerfGroup &operator=(const LinuxPerfGroup &) = delete;

    ~LinuxPerfGroup()
    {
        if (instructions_fd_ >= 0) {
            close(instructions_fd_);
        }
        if (cycles_fd_ >= 0) {
            close(cycles_fd_);
        }
    }

    bool open(std::string *error)
    {
        perf_event_attr cycles{};
        cycles.type = PERF_TYPE_HARDWARE;
        cycles.size = sizeof(cycles);
        cycles.config = PERF_COUNT_HW_CPU_CYCLES;
        cycles.disabled = 1;
        cycles.pinned = 1;
        cycles.exclude_kernel = 1;
        cycles.exclude_hv = 1;
        cycles.read_format = PERF_FORMAT_GROUP;

        cycles_fd_ = static_cast<int>(syscall(__NR_perf_event_open, &cycles,
                                              0, -1, -1, 0));
        if (cycles_fd_ < 0) {
            *error = std::string("perf_event_open(cycles): ") + std::strerror(errno);
            return false;
        }

        perf_event_attr instructions = cycles;
        instructions.config = PERF_COUNT_HW_INSTRUCTIONS;
        instructions.disabled = 0;
        instructions.read_format = 0;
        instructions_fd_ = static_cast<int>(syscall(__NR_perf_event_open, &instructions,
                                                    0, -1, cycles_fd_, 0));
        if (instructions_fd_ < 0) {
            *error = std::string("perf_event_open(instructions): ") + std::strerror(errno);
            close(cycles_fd_);
            cycles_fd_ = -1;
            return false;
        }
        return true;
    }

    bool begin(std::string *error)
    {
        if (ioctl(cycles_fd_, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) != 0 ||
            ioctl(cycles_fd_, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP) != 0) {
            *error = std::string("perf ioctl(enable/reset): ") + std::strerror(errno);
            return false;
        }
        return true;
    }

    bool end(CounterReading *reading, std::string *error)
    {
        if (ioctl(cycles_fd_, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) != 0) {
            *error = std::string("perf ioctl(disable): ") + std::strerror(errno);
            return false;
        }

        std::uint64_t values[3] = {};
        const ssize_t bytes = read(cycles_fd_, values, sizeof(values));
        if (bytes != static_cast<ssize_t>(sizeof(values)) || values[0] != 2) {
            *error = std::string("perf read(group): ") +
                     (bytes < 0 ? std::strerror(errno) : "unexpected group format");
            return false;
        }
        reading->cycles = values[1];
        reading->instructions = values[2];
        return true;
    }

private:
    int cycles_fd_ = -1;
    int instructions_fd_ = -1;
};

#endif  // defined(__linux__) && SME_BENCH_COMPILED

#if SME_BENCH_COMPILED

struct Sample {
    double elapsed_seconds = 0.0;
#if defined(__linux__)
    bool has_perf_counters = false;
    CounterReading counters;
#endif
};

bool sample_is_better(const Sample &candidate, const Sample &current)
{
#if defined(__linux__)
    if (candidate.has_perf_counters != current.has_perf_counters) {
        return candidate.has_perf_counters;
    }
    if (candidate.has_perf_counters && current.has_perf_counters) {
        return candidate.counters.cycles < current.counters.cycles;
    }
#endif
    return candidate.elapsed_seconds < current.elapsed_seconds;
}

Sample run_samples(SmeKernel kernel, const Options &options, Buffers *buffers
#if defined(__linux__)
                   , LinuxPerfGroup *perf_group, bool perf_available
#endif
)
{
    const std::uint64_t warmup_iterations =
        options.iterations < 10000 ? options.iterations : 10000;
    kernel(warmup_iterations, buffers->input);

    Sample best{};
    bool have_best = false;
    for (unsigned int trial = 0; trial < options.trials; ++trial) {
        Sample sample{};

#if defined(__linux__)
        std::string measurement_error;
        if (perf_available && !perf_group->begin(&measurement_error)) {
            std::cerr << "Note: " << measurement_error << "; disabling PMU IPC for this sample.\n";
        }
        const bool count_this_trial = perf_available && measurement_error.empty();
#endif

        const auto start = std::chrono::steady_clock::now();
        kernel(options.iterations, buffers->input);
        const auto end = std::chrono::steady_clock::now();
        sample.elapsed_seconds = std::chrono::duration<double>(end - start).count();

#if defined(__linux__)
        if (count_this_trial && perf_group->end(&sample.counters, &measurement_error)) {
            sample.has_perf_counters = true;
        } else if (count_this_trial) {
            std::cerr << "Note: " << measurement_error << "; this sample has no PMU IPC.\n";
        }
#endif

        if (!have_best || sample_is_better(sample, best)) {
            best = sample;
            have_best = true;
        }
    }
    return best;
}

double target_ipc(const Options &options, const RatioSpec &spec, const Sample &sample,
                  bool *is_hardware_measurement)
{
    const std::uint64_t target_instructions =
        options.iterations * (kBfmopaPerIteration + spec.ld1h_per_iteration);
#if defined(__linux__)
    if (sample.has_perf_counters && sample.counters.cycles != 0) {
        *is_hardware_measurement = true;
        return static_cast<double>(target_instructions) /
               static_cast<double>(sample.counters.cycles);
    }
#endif
    *is_hardware_measurement = false;
    if (options.core_ghz > 0.0) {
        return static_cast<double>(target_instructions) /
               (sample.elapsed_seconds * options.core_ghz * 1.0e9);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

void print_result(const Options &options, const RatioSpec &spec, const Sample &best)
{
    const std::uint64_t bfmopa = options.iterations * kBfmopaPerIteration;
    const std::uint64_t ld1h = options.iterations * spec.ld1h_per_iteration;
    const std::uint64_t target_instructions = bfmopa + ld1h;
    bool hardware_ipc = false;
    const double ipc = target_ipc(options, spec, best, &hardware_ipc);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\nBest of " << options.trials << " trials\n";
    std::cout << "  LD1H:BFMOPA ratio:       " << spec.ratio_text << ":1\n";
    std::cout << "  ST1W:                     0 (removed)\n";
    std::cout << "  elapsed:                 " << best.elapsed_seconds * 1.0e3 << " ms\n";
    std::cout << "  BFMOPA:                  " << bfmopa << "\n";
    std::cout << "  LD1H:                    " << ld1h << "\n";
    std::cout << "  target instructions:     " << target_instructions << "\n";
    std::cout << "  BFMOPA throughput:       "
              << static_cast<double>(bfmopa) / best.elapsed_seconds / 1.0e6
              << " Minst/s\n";
    std::cout << "  LD1H throughput:         "
              << static_cast<double>(ld1h) / best.elapsed_seconds / 1.0e6
              << " Minst/s\n";
    std::cout << "  target throughput:       "
              << static_cast<double>(target_instructions) / best.elapsed_seconds / 1.0e6
              << " Minst/s\n";

#if defined(__linux__)
    if (best.has_perf_counters) {
        std::cout << "  PMU cycles:               " << best.counters.cycles << "\n";
        std::cout << "  PMU retired instructions: " << best.counters.instructions << "\n";
        std::cout << "  target-only IPC:          " << ipc << "\n";
        std::cout << "  whole-kernel IPC:         "
                  << static_cast<double>(best.counters.instructions) /
                         static_cast<double>(best.counters.cycles)
                  << "\n";
        return;
    }
#endif

    if (std::isfinite(ipc)) {
        std::cout << "  estimated target IPC @ " << options.core_ghz << " GHz: "
                  << ipc << "\n";
        std::cout << "  (estimate only: wall time includes scheduling and the core may DVFS.)\n";
    } else {
        std::cout << "  IPC unavailable: no supported hardware counter was opened.\n";
    }
}

struct GridResult {
    const RatioSpec *spec;
    Sample sample;
};

void print_grid_summary(const Options &options, const std::vector<GridResult> &results)
{
    std::cout << "\nGrid summary (no ST1W; best of " << options.trials << " per ratio)\n";
    std::cout << std::left << std::setw(10) << "L:C"
              << std::right << std::setw(10) << "LD1H"
              << std::setw(10) << "BFMOPA"
              << std::setw(12) << "ms"
              << std::setw(14) << "LD1H M/s"
              << std::setw(16) << "BFMOPA M/s"
              << std::setw(14) << "target IPC" << "\n";

    for (const GridResult &result : results) {
        const std::uint64_t bfmopa = options.iterations * kBfmopaPerIteration;
        const std::uint64_t ld1h = options.iterations * result.spec->ld1h_per_iteration;
        bool hardware_ipc = false;
        const double ipc = target_ipc(options, *result.spec, result.sample, &hardware_ipc);

        std::cout << std::left << std::setw(10)
                  << (std::string(result.spec->ratio_text) + ":1")
                  << std::right << std::setw(10) << ld1h
                  << std::setw(10) << bfmopa
                  << std::setw(12) << std::fixed << std::setprecision(3)
                  << result.sample.elapsed_seconds * 1.0e3
                  << std::setw(14)
                  << static_cast<double>(ld1h) / result.sample.elapsed_seconds / 1.0e6
                  << std::setw(16)
                  << static_cast<double>(bfmopa) / result.sample.elapsed_seconds / 1.0e6;
        if (std::isfinite(ipc)) {
            std::cout << std::setw(14) << ipc;
        } else {
            std::cout << std::setw(14) << "-";
        }
        std::cout << "\n";
    }

#if defined(__linux__)
    bool any_hardware_ipc = false;
    for (const GridResult &result : results) {
        any_hardware_ipc = any_hardware_ipc || result.sample.has_perf_counters;
    }
    if (any_hardware_ipc) {
        std::cout << "target IPC uses PMU cycles; numerator is exactly LD1H + BFMOPA.\n";
        return;
    }
#endif
    if (options.core_ghz > 0.0) {
        std::cout << "target IPC is estimated at " << options.core_ghz
                  << " GHz; it is not a PMU measurement.\n";
    } else {
        std::cout << "target IPC is unavailable without PMU access or --core-ghz.\n";
    }
}

#endif  // SME_BENCH_COMPILED

}  // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

#if !SME_BENCH_COMPILED
    std::cerr << "This binary was not compiled with AArch64 SME enabled.\n"
              << "Rebuild on AArch64 with -march=armv9.2-a+sme.\n";
    return 77;
#else
#if defined(__linux__)
    if (!pin_to_cpu(options.cpu) && options.cpu >= 0) {
        return 2;
    }
#elif defined(__APPLE__)
    if (options.cpu >= 0) {
        std::cerr << "Note: macOS has no supported per-thread CPU-affinity API; --cpu is ignored.\n";
    }
#else
    if (options.cpu >= 0) {
        std::cerr << "Note: --cpu is only implemented on Linux.\n";
    }
#endif

    std::cout << "SME BFMOPA + LD1H load/compute IPC microbenchmark (no ST1W)\n";
#if defined(__APPLE__)
    std::cout << "Platform: macOS\n";
#elif defined(__linux__)
    std::cout << "Platform: Linux\n";
#else
    std::cout << "Platform: other AArch64 OS\n";
#endif
    std::cout << "Checking that this CPU and OS may execute SME BFMOPA...\n";
    if (!run_sme_probe(kernel_for(options.pattern))) {
        std::cerr << "SME BFMOPA is unavailable (or disabled by this OS/process); no benchmark was run.\n";
        return 77;
    }

    Buffers buffers{};
    for (std::size_t index = 0; index < kInputElements; ++index) {
        buffers.input[index] = 0x3f80;  // BF16 representation of 1.0.
    }

#if defined(__linux__)
    LinuxPerfGroup perf_group;
    std::string perf_error;
    const bool perf_available = perf_group.open(&perf_error);
    if (!perf_available) {
        std::cerr << "Note: " << perf_error << "; using wall-clock throughput only.\n";
    }
#endif

    const RatioSpec *selected = options.grid ? kRatioSpecs : &spec_for(options.pattern);
    const std::size_t selected_count = options.grid ? kRatioSpecCount : 1;
    std::vector<GridResult> results;
    results.reserve(selected_count);

    for (std::size_t index = 0; index < selected_count; ++index) {
        Options current = options;
        current.pattern = selected[index].pattern;
        const Sample best = run_samples(kernel_for(current.pattern), current, &buffers
#if defined(__linux__)
                                        , &perf_group, perf_available
#endif
        );
        if (options.grid) {
            results.push_back({&selected[index], best});
        } else {
            print_result(current, selected[index], best);
        }
    }

    if (options.grid) {
        print_grid_summary(options, results);
    }
    return 0;
#endif
}
