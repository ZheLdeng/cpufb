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

// Same-set lines are placed at multiples of this unit.  It is a multiple of
// every practical L1 way size (sets x line size, 4 KiB to 64 KiB), so no
// topology input is needed.
const size_t kSlotBytes = 64 * 1024;
const int kSlotCount = 1024;
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

// Links lines order[0] -> order[1] -> ... -> order[0].  Line i lives in slot
// slots[i]; with a non-zero skew it is moved (i + 1) lines into the slot, which
// puts every control line in a different set while keeping it next to its
// test line.  Test and control rings therefore coexist.
uint64_t build_ring(uint64_t *base, const std::vector<int> &order,
    const std::vector<int> &slots, size_t skew_words)
{
    const size_t slot_words = kSlotBytes / sizeof(uint64_t);
    const size_t count = order.size();
    auto word_index = [&](int line) {
        return static_cast<uint64_t>(slots[line]) * slot_words +
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

namespace {

// Ways observed with one placement of the same-set lines, or 0 when no
// conflict transition was seen.
int probe_with_placement(uint64_t *base, const std::vector<int> &slots,
    size_t skew_words, const char *placement, bool debug)
{
    int pending = 0; // line count of an unconfirmed conflict, 0 if none
    for (int lines = 2; lines <= kMaxLines; ++lines) {
        std::vector<int> order(lines);
        for (int i = 0; i < lines; ++i) order[i] = i;
        // A shuffled ring keeps stride prefetchers from following it.
        std::mt19937 generator(0x9e3779b9U + lines);
        std::shuffle(order.begin(), order.end(), generator);

        const uint64_t control_first =
            build_ring(base, order, slots, skew_words);
        const uint64_t test_first = build_ring(base, order, slots, 0);
        const double ratio = conflict_ratio(base, test_first, control_first);
        if (debug)
            std::fprintf(stderr,
                "associativity probe [%s]: lines=%d ratio=%.3f\n", placement,
                lines, ratio);

        // A real conflict persists for every larger ring; a noise spike does
        // not.  Accept the transition only when the next size confirms it.
        if (ratio >= kConflictRatio) {
            if (pending != 0) return pending - 1;
            pending = lines;
        } else {
            pending = 0;
        }
    }
    return pending == kMaxLines ? pending - 1 : 0;
}

} // namespace

int probe_l1_associativity(int cacheline_bytes)
{
    const size_t line = cacheline_bytes > 0 ? cacheline_bytes : 64;
    const size_t bytes = kSlotBytes * kSlotCount + line * (kMaxLines + 1);

    // Only the touched pages of this 64 MiB range are ever committed.
    void *allocation = nullptr;
    if (posix_memalign(&allocation, kSlotBytes, bytes) != 0) return 0;
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
    // Huge pages would commit 2 MiB for every touched line.
    (void)madvise(allocation, bytes, MADV_NOHUGEPAGE);
#endif
    uint64_t *base = static_cast<uint64_t *>(allocation);
    const size_t skew_words = line / sizeof(uint64_t);
    const bool debug = debug_enabled();

    // Lines that share a set can also collide in an address-hashed way
    // predictor or partial tag, and which addresses collide is specific to the
    // core: a Kunpeng 920F is clean with a regular 1 MiB stride but conflicts
    // at three lines with random multiples of 64 KiB, while an Apple M4
    // conflicts at three lines with the 1 MiB stride.  Such a collision can
    // only bring the transition forward, never delay it, so the largest
    // result over several placements is the associativity.
    int detected = 0;
    std::vector<int> slots(kMaxLines);
    for (int i = 0; i < kMaxLines; ++i) slots[i] = i * 16; // 1 MiB stride
    detected = std::max(detected,
        probe_with_placement(base, slots, skew_words, "1 MiB stride", debug));

    const unsigned seeds[] = {0x43505546U, 0x9e3779b9U, 0x7f4a7c15U};
    for (unsigned seed : seeds) {
        std::vector<int> shuffled(kSlotCount);
        for (int i = 0; i < kSlotCount; ++i) shuffled[i] = i;
        std::mt19937 slot_generator(seed);
        std::shuffle(shuffled.begin(), shuffled.end(), slot_generator);
        shuffled.resize(kMaxLines);
        detected = std::max(detected,
            probe_with_placement(base, shuffled, skew_words, "random", debug));
    }

    std::free(allocation);
    return detected;
}

} // namespace cpufb
