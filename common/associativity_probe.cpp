#include "associativity_probe.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <vector>

#ifndef _WIN32
#include <sys/mman.h>
#endif

namespace cpufb {

namespace {

// A power of two that is a multiple of every practical L1 way size
// (sets x line size, e.g. 4 KiB to 64 KiB), so no topology input is needed.
const size_t kConflictStride = 1024 * 1024;
const int kMaxLines = 48;
const int kRingSteps = 200000;
const int kRepeats = 7;
// Thrashing one L1 set turns every access into an L2 hit, which costs well
// over twice an L1 hit; 25% keeps clear of timing noise while surviving a
// large common translation cost in both rings.
const double kConflictRatio = 1.25;

volatile uint64_t g_associativity_probe_sink = 0;

double time_ring(const uint64_t *base, uint64_t first)
{
    timespec start, end;
    uint64_t next = first;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (int step = 0; step < kRingSteps; ++step) next = base[next];
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    g_associativity_probe_sink ^= next;
    return end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

// Links lines order[0] -> order[1] -> ... -> order[0]; line i lives at word
// index i * stride_words + i * skew_words.
uint64_t build_ring(uint64_t *base, const std::vector<int> &order,
    size_t stride_words, size_t skew_words)
{
    const size_t count = order.size();
    for (size_t i = 0; i < count; ++i) {
        const uint64_t from = order[i] * (stride_words + skew_words);
        const uint64_t to =
            order[(i + 1) % count] * (stride_words + skew_words);
        base[from] = to;
    }
    return order[0] * (stride_words + skew_words);
}

double median_ring_time(const uint64_t *base, uint64_t first)
{
    time_ring(base, first);
    std::vector<double> samples;
    samples.reserve(kRepeats);
    for (int repeat = 0; repeat < kRepeats; ++repeat)
        samples.push_back(time_ring(base, first));
    std::sort(samples.begin(), samples.end());
    // Interrupts only lengthen a sample, so the median is robust.
    return samples[samples.size() / 2];
}

bool debug_enabled()
{
    const char *value = std::getenv("CPUFB_DEBUG_ASSOCIATIVITY");
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

} // namespace

int probe_l1_associativity(int cacheline_bytes)
{
    const size_t line = cacheline_bytes > 0 ? cacheline_bytes : 64;
    const size_t bytes = kConflictStride * kMaxLines + line * kMaxLines;

    void *allocation = nullptr;
    if (posix_memalign(&allocation, kConflictStride, bytes) != 0) return 0;
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
    // Only kMaxLines pages are touched; huge pages would commit 2 MiB each.
    (void)madvise(allocation, bytes, MADV_NOHUGEPAGE);
#endif
    uint64_t *base = static_cast<uint64_t *>(allocation);

    const size_t stride_words = kConflictStride / sizeof(uint64_t);
    const size_t skew_words = line / sizeof(uint64_t);
    const bool debug = debug_enabled();
    int detected = 0;

    for (int lines = 2; lines <= kMaxLines; ++lines) {
        std::vector<int> order(lines);
        for (int i = 0; i < lines; ++i) order[i] = i;
        // A shuffled ring keeps stride prefetchers from following it.
        std::mt19937 generator(0x9e3779b9U + lines);
        std::shuffle(order.begin(), order.end(), generator);

        const uint64_t control_first =
            build_ring(base, order, stride_words, skew_words);
        const double control_time = median_ring_time(base, control_first);
        const uint64_t test_first = build_ring(base, order, stride_words, 0);
        const double test_time = median_ring_time(base, test_first);

        const double ratio =
            control_time > 0.0 ? test_time / control_time : 0.0;
        if (debug)
            std::fprintf(stderr, "associativity probe: lines=%d ratio=%.3f\n",
                lines, ratio);
        if (ratio >= kConflictRatio) {
            detected = lines - 1;
            break;
        }
    }

    std::free(allocation);
    return detected;
}

} // namespace cpufb
