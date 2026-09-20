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
// Long enough that one sample is milliseconds even for an L1-resident ring;
// a 0.2 ms sample was dominated by clock ramps and migrations on an unpinned
// macOS thread and produced a false conflict at three lines.
const int kRingSteps = 2000000;
const int kRepeats = 5;
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
// index i * stride_words + (i + 1) * skew_words.  With a non-zero skew every
// control node is distinct from every test node, so both rings coexist.
uint64_t build_ring(uint64_t *base, const std::vector<int> &order,
    size_t stride_words, size_t skew_words)
{
    const size_t count = order.size();
    auto word_index = [&](int line) {
        return static_cast<uint64_t>(line) * stride_words +
            (skew_words ? (line + 1) * skew_words : 0);
    };
    for (size_t i = 0; i < count; ++i)
        base[word_index(order[i])] = word_index(order[(i + 1) % count]);
    return word_index(order[0]);
}

// Interrupts, migrations and clock ramps can only lengthen a sample, so the
// minimum is the estimate.  Test and control samples are interleaved so that
// a slow drift of the core clock affects both rings alike.
double conflict_ratio(
    const uint64_t *base, uint64_t test_first, uint64_t control_first)
{
    time_ring(base, test_first);
    time_ring(base, control_first);
    double test_time = 0.0;
    double control_time = 0.0;
    for (int repeat = 0; repeat < kRepeats; ++repeat) {
        const double test_sample = time_ring(base, test_first);
        const double control_sample = time_ring(base, control_first);
        if (test_time == 0.0 || test_sample < test_time)
            test_time = test_sample;
        if (control_time == 0.0 || control_sample < control_time)
            control_time = control_sample;
    }
    return control_time > 0.0 ? test_time / control_time : 0.0;
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
    const size_t bytes = kConflictStride * kMaxLines + line * (kMaxLines + 1);

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

    int pending = 0; // line count of an unconfirmed conflict, 0 if none
    for (int lines = 2; lines <= kMaxLines; ++lines) {
        std::vector<int> order(lines);
        for (int i = 0; i < lines; ++i) order[i] = i;
        // A shuffled ring keeps stride prefetchers from following it.
        std::mt19937 generator(0x9e3779b9U + lines);
        std::shuffle(order.begin(), order.end(), generator);

        const uint64_t control_first =
            build_ring(base, order, stride_words, skew_words);
        const uint64_t test_first = build_ring(base, order, stride_words, 0);
        const double ratio = conflict_ratio(base, test_first, control_first);
        if (debug)
            std::fprintf(stderr, "associativity probe: lines=%d ratio=%.3f\n",
                lines, ratio);

        // A real conflict persists for every larger ring; a noise spike does
        // not.  Accept the transition only when the next size confirms it.
        if (ratio >= kConflictRatio) {
            if (pending != 0) {
                detected = pending - 1;
                break;
            }
            pending = lines;
        } else {
            pending = 0;
        }
    }
    if (detected == 0 && pending == kMaxLines) detected = pending - 1;

    std::free(allocation);
    return detected;
}

} // namespace cpufb
