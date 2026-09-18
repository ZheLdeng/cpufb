// Linux/AArch64 SVE software-prefetch control experiment.
//
// Build and run on the 96-core NUMA node of the benchmark host:
//   g++ -O3 -std=c++17 -march=armv8.2-a+sve -pthread \
//       tmp/dram_prefetch_control.cpp -o /tmp/dram_prefetch_control
//   /tmp/dram_prefetch_control --cpus=96-191 \
//       --threads=1,8,16,24,32,48,96 --size-mib=384 --repetitions=7
//
// Every worker is pinned before allocation and first-touch, so each stream is
// private and local to the selected NUMA domain.  The SVE load body is the
// same 16 x ld1h stream used by cpufb's memory test.  The only difference
// between variants is a PRFM for every cache line consumed by the upcoming
// demand-load block (four on SVE128 and eight on SVE256).

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <asm/hwcap.h>
#include <pthread.h>
#include <sched.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#if !defined(__aarch64__)
#error "This experiment requires Linux/AArch64."
#endif

namespace {

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
constexpr std::size_t kCacheLineBytes = 64;
// The farthest SVE256 full-coverage hint is +64 cache lines plus its seventh
// cache line.  Keep one more line mapped and first-touched beyond it.
constexpr std::size_t kMaxPrefetchBytes = 72 * kCacheLineBytes;
constexpr int kLoadsPerBlock = 16;

struct Options
{
    std::vector<int> cpus;
    std::vector<std::size_t> thread_counts;
    std::uint64_t bytes_per_stream = 384 * kMiB;
    int repetitions = 7;
    int passes_per_sample = 1;
};

enum class PrefetchKind
{
    None,
    L1Keep,
    L2Keep,
    L1Stream,
    L2Stream,
};

struct Variant
{
    const char *name;
    PrefetchKind kind;
    std::size_t distance_bytes;
};

constexpr Variant kVariants[] = {
    {"none", PrefetchKind::None, 0},
    {"full-pldl1keep+8cl", PrefetchKind::L1Keep, 8 * kCacheLineBytes},
    {"full-pldl1keep+16cl", PrefetchKind::L1Keep, 16 * kCacheLineBytes},
    {"full-pldl1keep+32cl", PrefetchKind::L1Keep, 32 * kCacheLineBytes},
    {"full-pldl1keep+64cl", PrefetchKind::L1Keep, 64 * kCacheLineBytes},
    {"full-pldl2keep+8cl", PrefetchKind::L2Keep, 8 * kCacheLineBytes},
    {"full-pldl2keep+16cl", PrefetchKind::L2Keep, 16 * kCacheLineBytes},
    {"full-pldl2keep+32cl", PrefetchKind::L2Keep, 32 * kCacheLineBytes},
    {"full-pldl2keep+64cl", PrefetchKind::L2Keep, 64 * kCacheLineBytes},
    {"full-pldl1strm+32cl", PrefetchKind::L1Stream, 32 * kCacheLineBytes},
    {"full-pldl2strm+8cl", PrefetchKind::L2Stream, 8 * kCacheLineBytes},
    {"full-pldl2strm+16cl", PrefetchKind::L2Stream, 16 * kCacheLineBytes},
    {"full-pldl2strm+32cl", PrefetchKind::L2Stream, 32 * kCacheLineBytes},
    {"full-pldl2strm+64cl", PrefetchKind::L2Stream, 64 * kCacheLineBytes},
};

struct SharedState
{
    pthread_barrier_t ready;
    pthread_barrier_t start;
    pthread_barrier_t finish;
    const Variant *variant = nullptr;
    std::size_t vector_bytes = 0;
    int passes_per_sample = 1;
    bool stop = false;
};

struct Worker
{
    int cpu = -1;
    std::size_t stream_bytes = 0;
    std::size_t allocation_bytes = 0;
    std::size_t blocks = 0;
    SharedState *shared = nullptr;
    pthread_t thread{};
    std::uint8_t *data = nullptr;
    timespec start{};
    timespec end{};
    std::string error;
};

struct Result
{
    std::size_t threads = 0;
    const Variant *variant = nullptr;
    double median_gb_per_second = 0.0;
    double minimum_gb_per_second = 0.0;
    double maximum_gb_per_second = 0.0;
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

std::size_t sve_vector_bytes()
{
    std::size_t bytes = 0;
    asm volatile("cntb %0" : "=r"(bytes));
    return bytes;
}

bool pin_to_cpu(int cpu, std::string &error)
{
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
    return true;
}

// One iteration consumes 16 SVE vectors.  The z-register values are not
// semantically used, but asm volatile plus a memory clobber keeps these as
// real demand loads rather than allowing the compiler to remove the stream.
#define SVE_LOAD_BODY                                                        \
    "addvl x9, %[ptr], #8\n\t"                                              \
    "ld1h z0.h,  p0/z, [%[ptr]]\n\t"                                       \
    "ld1h z1.h,  p0/z, [%[ptr], #1, MUL VL]\n\t"                          \
    "ld1h z2.h,  p0/z, [%[ptr], #2, MUL VL]\n\t"                          \
    "ld1h z3.h,  p0/z, [%[ptr], #3, MUL VL]\n\t"                          \
    "ld1h z4.h,  p0/z, [%[ptr], #4, MUL VL]\n\t"                          \
    "ld1h z5.h,  p0/z, [%[ptr], #5, MUL VL]\n\t"                          \
    "ld1h z6.h,  p0/z, [%[ptr], #6, MUL VL]\n\t"                          \
    "ld1h z7.h,  p0/z, [%[ptr], #7, MUL VL]\n\t"                          \
    "ld1h z16.h, p0/z, [x9]\n\t"                                          \
    "ld1h z17.h, p0/z, [x9, #1, MUL VL]\n\t"                              \
    "ld1h z18.h, p0/z, [x9, #2, MUL VL]\n\t"                              \
    "ld1h z19.h, p0/z, [x9, #3, MUL VL]\n\t"                              \
    "ld1h z20.h, p0/z, [x9, #4, MUL VL]\n\t"                              \
    "ld1h z21.h, p0/z, [x9, #5, MUL VL]\n\t"                              \
    "ld1h z22.h, p0/z, [x9, #6, MUL VL]\n\t"                              \
    "ld1h z23.h, p0/z, [x9, #7, MUL VL]\n\t"                              \
    "addvl %[ptr], %[ptr], #16\n\t"

#define SVE_CLOBBERS                                                         \
    "p0", "x9", "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", \
    "z16", "z17", "z18", "z19", "z20", "z21", "z22", "z23",         \
    "cc", "memory"

#define DEFINE_STREAM_KERNEL(function_name, prefetch_asm)                   \
    __attribute__((noinline)) void function_name(std::uint8_t *data,        \
        std::size_t blocks, int passes)                                      \
    {                                                                         \
        for (int pass = 0; pass < passes; ++pass) {                          \
            std::uint8_t *ptr = data;                                        \
            std::size_t count = blocks;                                      \
            asm volatile(                                                     \
                "ptrue p0.h\n\t"                                           \
                "1:\n\t"                                                  \
                prefetch_asm                                                  \
                SVE_LOAD_BODY                                                 \
                "subs %[count], %[count], #1\n\t"                          \
                "b.ne 1b\n\t"                                             \
                : [ptr] "+r"(ptr), [count] "+r"(count)                    \
                :                                                               \
                : SVE_CLOBBERS);                                             \
        }                                                                     \
    }

DEFINE_STREAM_KERNEL(stream_none, "")
// Sixteen demand loads consume 4 cache lines with SVE128 and 8 cache lines
// with SVE256.  Use a matching number of prefetches, so a "full" variant
// always covers every line in exactly one future demand block.
#define PRFM_LINE(hint, offset)                                              \
    "prfm " #hint ", [%[ptr], #" #offset "]\n\t"
#define PRFM4(hint, a, b, c, d)                                              \
    PRFM_LINE(hint, a) PRFM_LINE(hint, b) PRFM_LINE(hint, c)                \
    PRFM_LINE(hint, d)
#define PRFM8(hint, a, b, c, d, e, f, g, h)                                  \
    PRFM4(hint, a, b, c, d) PRFM4(hint, e, f, g, h)

DEFINE_STREAM_KERNEL(stream128_l1_keep_full_8cl,
    PRFM4(pldl1keep, 512, 576, 640, 704))
DEFINE_STREAM_KERNEL(stream128_l1_keep_full_16cl,
    PRFM4(pldl1keep, 1024, 1088, 1152, 1216))
DEFINE_STREAM_KERNEL(stream128_l1_keep_full_32cl,
    PRFM4(pldl1keep, 2048, 2112, 2176, 2240))
DEFINE_STREAM_KERNEL(stream128_l1_keep_full_64cl,
    PRFM4(pldl1keep, 4096, 4160, 4224, 4288))
DEFINE_STREAM_KERNEL(stream128_l2_keep_full_32cl,
    PRFM4(pldl2keep, 2048, 2112, 2176, 2240))
DEFINE_STREAM_KERNEL(stream128_l2_keep_full_8cl,
    PRFM4(pldl2keep, 512, 576, 640, 704))
DEFINE_STREAM_KERNEL(stream128_l2_keep_full_16cl,
    PRFM4(pldl2keep, 1024, 1088, 1152, 1216))
DEFINE_STREAM_KERNEL(stream128_l2_keep_full_64cl,
    PRFM4(pldl2keep, 4096, 4160, 4224, 4288))
DEFINE_STREAM_KERNEL(stream128_l1_strm_full_32cl,
    PRFM4(pldl1strm, 2048, 2112, 2176, 2240))
DEFINE_STREAM_KERNEL(stream128_l2_strm_full_32cl,
    PRFM4(pldl2strm, 2048, 2112, 2176, 2240))
DEFINE_STREAM_KERNEL(stream128_l2_strm_full_8cl,
    PRFM4(pldl2strm, 512, 576, 640, 704))
DEFINE_STREAM_KERNEL(stream128_l2_strm_full_16cl,
    PRFM4(pldl2strm, 1024, 1088, 1152, 1216))
DEFINE_STREAM_KERNEL(stream128_l2_strm_full_64cl,
    PRFM4(pldl2strm, 4096, 4160, 4224, 4288))

DEFINE_STREAM_KERNEL(stream256_l1_keep_full_8cl,
    PRFM8(pldl1keep, 512, 576, 640, 704, 768, 832, 896, 960))
DEFINE_STREAM_KERNEL(stream256_l1_keep_full_16cl,
    PRFM8(pldl1keep, 1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472))
DEFINE_STREAM_KERNEL(stream256_l1_keep_full_32cl,
    PRFM8(pldl1keep, 2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496))
DEFINE_STREAM_KERNEL(stream256_l1_keep_full_64cl,
    PRFM8(pldl1keep, 4096, 4160, 4224, 4288, 4352, 4416, 4480, 4544))
DEFINE_STREAM_KERNEL(stream256_l2_keep_full_32cl,
    PRFM8(pldl2keep, 2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496))
DEFINE_STREAM_KERNEL(stream256_l2_keep_full_8cl,
    PRFM8(pldl2keep, 512, 576, 640, 704, 768, 832, 896, 960))
DEFINE_STREAM_KERNEL(stream256_l2_keep_full_16cl,
    PRFM8(pldl2keep, 1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472))
DEFINE_STREAM_KERNEL(stream256_l2_keep_full_64cl,
    PRFM8(pldl2keep, 4096, 4160, 4224, 4288, 4352, 4416, 4480, 4544))
DEFINE_STREAM_KERNEL(stream256_l1_strm_full_32cl,
    PRFM8(pldl1strm, 2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496))
DEFINE_STREAM_KERNEL(stream256_l2_strm_full_32cl,
    PRFM8(pldl2strm, 2048, 2112, 2176, 2240, 2304, 2368, 2432, 2496))
DEFINE_STREAM_KERNEL(stream256_l2_strm_full_8cl,
    PRFM8(pldl2strm, 512, 576, 640, 704, 768, 832, 896, 960))
DEFINE_STREAM_KERNEL(stream256_l2_strm_full_16cl,
    PRFM8(pldl2strm, 1024, 1088, 1152, 1216, 1280, 1344, 1408, 1472))
DEFINE_STREAM_KERNEL(stream256_l2_strm_full_64cl,
    PRFM8(pldl2strm, 4096, 4160, 4224, 4288, 4352, 4416, 4480, 4544))

using StreamFunction = void (*)(std::uint8_t *, std::size_t, int);

StreamFunction stream_function(const Variant &variant,
    std::size_t vector_bytes)
{
    if (variant.kind == PrefetchKind::None) return stream_none;
    const bool sve256_or_wider = vector_bytes >= 32;
    if (variant.kind == PrefetchKind::L1Keep) {
        switch (variant.distance_bytes) {
        case 8 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l1_keep_full_8cl :
                stream128_l1_keep_full_8cl;
        case 16 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l1_keep_full_16cl :
                stream128_l1_keep_full_16cl;
        case 32 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l1_keep_full_32cl :
                stream128_l1_keep_full_32cl;
        case 64 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l1_keep_full_64cl :
                stream128_l1_keep_full_64cl;
        default: break;
        }
    }
    if (variant.kind == PrefetchKind::L2Keep) {
        switch (variant.distance_bytes) {
        case 8 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_keep_full_8cl :
                stream128_l2_keep_full_8cl;
        case 16 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_keep_full_16cl :
                stream128_l2_keep_full_16cl;
        case 32 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_keep_full_32cl :
                stream128_l2_keep_full_32cl;
        case 64 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_keep_full_64cl :
                stream128_l2_keep_full_64cl;
        default: break;
        }
    }
    if (variant.kind == PrefetchKind::L1Stream &&
        variant.distance_bytes == 32 * kCacheLineBytes) {
        return sve256_or_wider ? stream256_l1_strm_full_32cl :
            stream128_l1_strm_full_32cl;
    }
    if (variant.kind == PrefetchKind::L2Stream) {
        switch (variant.distance_bytes) {
        case 8 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_strm_full_8cl :
                stream128_l2_strm_full_8cl;
        case 16 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_strm_full_16cl :
                stream128_l2_strm_full_16cl;
        case 32 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_strm_full_32cl :
                stream128_l2_strm_full_32cl;
        case 64 * kCacheLineBytes:
            return sve256_or_wider ? stream256_l2_strm_full_64cl :
                stream128_l2_strm_full_64cl;
        default: break;
        }
    }
    return nullptr;
}

void *worker_main(void *opaque)
{
    Worker &worker = *static_cast<Worker *>(opaque);
    if (!pin_to_cpu(worker.cpu, worker.error)) goto ready;

    worker.data = static_cast<std::uint8_t *>(mmap(nullptr,
        worker.allocation_bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0));
    if (worker.data == MAP_FAILED) {
        worker.data = nullptr;
        worker.error = "mmap(" + std::to_string(worker.allocation_bytes) +
            "): " + std::strerror(errno);
        goto ready;
    }
    (void)madvise(worker.data, worker.allocation_bytes, MADV_HUGEPAGE);
    // First-touch happens after affinity is set, placing pages on this worker's
    // NUMA node.  The private allocation prevents cache-line sharing.
    std::memset(worker.data, 1, worker.allocation_bytes);
    stream_none(worker.data, worker.blocks, 1);

ready:
    (void)pthread_barrier_wait(&worker.shared->ready);
    while (true) {
        (void)pthread_barrier_wait(&worker.shared->start);
        if (worker.shared->stop) break;

        const StreamFunction function = stream_function(*worker.shared->variant,
            worker.shared->vector_bytes);
        clock_gettime(CLOCK_MONOTONIC_RAW, &worker.start);
        function(worker.data, worker.blocks, worker.shared->passes_per_sample);
        clock_gettime(CLOCK_MONOTONIC_RAW, &worker.end);
        (void)pthread_barrier_wait(&worker.shared->finish);
    }

    if (worker.data != nullptr)
        munmap(worker.data, worker.allocation_bytes);
    return nullptr;
}

bool parse_positive(const std::string &text, std::uint64_t &value)
{
    if (text.empty()) return false;
    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || parsed == 0)
        return false;
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_nonnegative(const std::string &text, std::uint64_t &value)
{
    if (text.empty()) return false;
    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') return false;
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_cpu_range(const std::string &text, std::vector<int> &cpus)
{
    const std::size_t dash = text.find('-');
    if (dash == std::string::npos) return false;
    std::uint64_t first = 0;
    std::uint64_t last = 0;
    if (!parse_nonnegative(text.substr(0, dash), first) ||
        !parse_nonnegative(text.substr(dash + 1), last) || first > last ||
        last >= static_cast<std::uint64_t>(CPU_SETSIZE)) {
        return false;
    }
    cpus.clear();
    for (std::uint64_t cpu = first; cpu <= last; ++cpu)
        cpus.push_back(static_cast<int>(cpu));
    return true;
}

bool parse_thread_counts(const std::string &text,
    std::vector<std::size_t> &counts)
{
    counts.clear();
    std::size_t begin = 0;
    while (begin < text.size()) {
        const std::size_t end = text.find(',', begin);
        std::uint64_t value = 0;
        if (!parse_positive(text.substr(begin, end - begin), value))
            return false;
        counts.push_back(static_cast<std::size_t>(value));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return !counts.empty();
}

void print_usage(const char *program)
{
    std::cerr << "Usage: " << program << " [options]\n"
              << "  --cpus=96-191                 CPU range, one NUMA node\n"
              << "  --threads=1,8,16,24,32,48,96 Thread-count sweep\n"
              << "  --size-mib=384                Per-stream DRAM workset\n"
              << "  --repetitions=7               Samples per variant\n"
              << "  --passes=1                    Full stream passes per sample\n";
}

bool parse_options(int argc, char **argv, Options &options)
{
    if (!parse_cpu_range("96-191", options.cpus) ||
        !parse_thread_counts("1,8,16,24,32,48,96", options.thread_counts)) {
        return false;
    }
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        const std::size_t equals = argument.find('=');
        const std::string name = argument.substr(0, equals);
        const std::string value = equals == std::string::npos ? "" :
            argument.substr(equals + 1);
        if (name == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        }
        std::uint64_t parsed = 0;
        if (name == "--cpus") {
            if (!parse_cpu_range(value, options.cpus)) return false;
        } else if (name == "--threads") {
            if (!parse_thread_counts(value, options.thread_counts)) return false;
        } else if (name == "--size-mib") {
            if (!parse_positive(value, parsed) ||
                parsed > std::numeric_limits<std::uint64_t>::max() / kMiB) {
                return false;
            }
            options.bytes_per_stream = parsed * kMiB;
        } else if (name == "--repetitions") {
            if (!parse_positive(value, parsed) || parsed > INT_MAX) return false;
            options.repetitions = static_cast<int>(parsed);
        } else if (name == "--passes") {
            if (!parse_positive(value, parsed) || parsed > INT_MAX) return false;
            options.passes_per_sample = static_cast<int>(parsed);
        } else {
            return false;
        }
    }
    for (const std::size_t count : options.thread_counts) {
        if (count > options.cpus.size()) return false;
    }
    return true;
}

bool run_thread_count(const Options &options,
    std::size_t thread_count,
    std::size_t block_bytes,
    std::vector<Result> &results)
{
    const std::size_t vector_bytes = block_bytes / kLoadsPerBlock;
    if (vector_bytes != 16 && vector_bytes != 32) {
        std::cerr << "This full-coverage experiment currently supports "
                  << "SVE128 and SVE256, got " << vector_bytes * 8
                  << " bit SVE." << std::endl;
        return false;
    }
    const std::uint64_t aligned_stream_bytes =
        options.bytes_per_stream - options.bytes_per_stream % block_bytes;
    if (aligned_stream_bytes == 0 || aligned_stream_bytes >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        std::cerr << "Invalid stream size." << std::endl;
        return false;
    }
    if (aligned_stream_bytes > std::numeric_limits<std::size_t>::max() -
        kMaxPrefetchBytes) {
        std::cerr << "Stream allocation is too large." << std::endl;
        return false;
    }

    SharedState shared;
    shared.vector_bytes = vector_bytes;
    shared.passes_per_sample = options.passes_per_sample;
    if (pthread_barrier_init(&shared.ready, nullptr, thread_count + 1) != 0 ||
        pthread_barrier_init(&shared.start, nullptr, thread_count + 1) != 0 ||
        pthread_barrier_init(&shared.finish, nullptr, thread_count + 1) != 0) {
        std::cerr << "pthread_barrier_init failed." << std::endl;
        return false;
    }

    std::vector<Worker> workers(thread_count);
    for (std::size_t index = 0; index < thread_count; ++index) {
        Worker &worker = workers[index];
        worker.cpu = options.cpus[index];
        worker.stream_bytes = static_cast<std::size_t>(aligned_stream_bytes);
        worker.allocation_bytes = worker.stream_bytes + kMaxPrefetchBytes;
        worker.blocks = worker.stream_bytes / block_bytes;
        worker.shared = &shared;
        const int status = pthread_create(&worker.thread, nullptr,
            worker_main, &worker);
        if (status != 0) {
            std::cerr << "pthread_create: " << std::strerror(status)
                      << std::endl;
            // Existing workers are waiting at a barrier whose participant
            // count includes the failed launch.  Exiting avoids a deadlock
            // and lets the OS reclaim those incomplete experiment workers.
            std::exit(1);
        }
    }

    (void)pthread_barrier_wait(&shared.ready);
    for (const Worker &worker : workers) {
        if (!worker.error.empty()) {
            std::cerr << "CPU " << worker.cpu << ": " << worker.error
                      << std::endl;
            shared.stop = true;
            (void)pthread_barrier_wait(&shared.start);
            for (Worker &joined : workers) pthread_join(joined.thread, nullptr);
            pthread_barrier_destroy(&shared.ready);
            pthread_barrier_destroy(&shared.start);
            pthread_barrier_destroy(&shared.finish);
            return false;
        }
    }

    std::vector<std::vector<double> > samples(
        sizeof(kVariants) / sizeof(kVariants[0]));
    for (int repetition = 0; repetition < options.repetitions; ++repetition) {
        // Rotate the order so a warm system or frequency drift does not always
        // favor one variant.
        for (std::size_t order = 0;
             order < sizeof(kVariants) / sizeof(kVariants[0]); ++order) {
            const std::size_t variant_index =
                (order + static_cast<std::size_t>(repetition)) %
                (sizeof(kVariants) / sizeof(kVariants[0]));
            shared.variant = &kVariants[variant_index];
            (void)pthread_barrier_wait(&shared.start);
            (void)pthread_barrier_wait(&shared.finish);

            timespec first = workers.front().start;
            timespec last = workers.front().end;
            for (const Worker &worker : workers) {
                if (earlier(worker.start, first)) first = worker.start;
                if (earlier(last, worker.end)) last = worker.end;
            }
            const double seconds = elapsed_seconds(first, last);
            const double transferred_bytes = static_cast<double>(aligned_stream_bytes) *
                thread_count * options.passes_per_sample;
            const double bandwidth = transferred_bytes / seconds / 1.0e9;
            samples[variant_index].push_back(bandwidth);
        }
    }

    shared.stop = true;
    (void)pthread_barrier_wait(&shared.start);
    for (Worker &worker : workers) pthread_join(worker.thread, nullptr);
    pthread_barrier_destroy(&shared.ready);
    pthread_barrier_destroy(&shared.start);
    pthread_barrier_destroy(&shared.finish);

    for (std::size_t index = 0;
         index < sizeof(kVariants) / sizeof(kVariants[0]); ++index) {
        std::vector<double> &variant_samples = samples[index];
        std::sort(variant_samples.begin(), variant_samples.end());
        Result result;
        result.threads = thread_count;
        result.variant = &kVariants[index];
        result.minimum_gb_per_second = variant_samples.front();
        result.median_gb_per_second =
            variant_samples[variant_samples.size() / 2];
        result.maximum_gb_per_second = variant_samples.back();
        results.push_back(result);
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    if ((getauxval(AT_HWCAP) & HWCAP_SVE) == 0) {
        std::cerr << "SVE is required for this experiment." << std::endl;
        return 1;
    }

    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 1;
    }

    const std::size_t vector_bytes = sve_vector_bytes();
    const std::size_t block_bytes = vector_bytes * kLoadsPerBlock;
    std::cout << "SVE vector: " << vector_bytes * 8 << " bit; "
              << "load body: " << kLoadsPerBlock << " x ld1h = "
              << block_bytes << " B/block\n";
    std::cout << "Per-stream workset: " << options.bytes_per_stream / kMiB
              << " MiB; samples: " << options.repetitions
              << "; passes/sample: " << options.passes_per_sample << "\n";
    std::cout << "Threads,Variant,Median GB/s,Min GB/s,Max GB/s,Gain vs none\n";

    for (const std::size_t thread_count : options.thread_counts) {
        std::vector<Result> results;
        if (!run_thread_count(options, thread_count, block_bytes, results))
            return 1;
        const double baseline = results.front().median_gb_per_second;
        for (const Result &result : results) {
            const double gain = baseline > 0.0 ?
                (result.median_gb_per_second / baseline - 1.0) * 100.0 : 0.0;
            std::cout << result.threads << ',' << result.variant->name << ','
                      << std::fixed << std::setprecision(3)
                      << result.median_gb_per_second << ','
                      << result.minimum_gb_per_second << ','
                      << result.maximum_gb_per_second << ','
                      << std::showpos << gain << '%' << std::noshowpos << '\n';
        }
    }
    return 0;
}
