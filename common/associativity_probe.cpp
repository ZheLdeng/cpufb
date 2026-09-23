#include "associativity_probe.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
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

namespace {

// Line i of `order` lives at byte offsets[line]; a non-zero skew moves it
// (line + 1) skews further, which gives the control ring one set per line.
uint64_t build_ring_at(uint64_t *base, const std::vector<int> &order,
    const std::vector<size_t> &offsets, size_t skew_bytes)
{
    auto word_index = [&](int line) {
        return static_cast<uint64_t>(
            (offsets[line] + (line + 1) * skew_bytes) / sizeof(uint64_t));
    };
    const size_t count = order.size();
    for (size_t i = 0; i < count; ++i)
        base[word_index(order[i])] = word_index(order[(i + 1) % count]);
    return word_index(order[0]);
}

// Smallest power-of-two stride at which `lines` lines conflict, for one
// placement of their slots, or 0.  A conflict must persist at twice the
// stride, since every multiple of the way size maps to the same set.
size_t way_bytes_for_placement(uint64_t *base, const std::vector<int> &slots,
    size_t line, int lines, const char *placement, bool debug)
{
    std::vector<int> order(lines);
    for (int i = 0; i < lines; ++i) order[i] = i;
    std::mt19937 generator(0x9e3779b9U + lines);
    std::shuffle(order.begin(), order.end(), generator);

    auto conflicts = [&](size_t stride) {
        std::vector<size_t> offsets(lines);
        for (int i = 0; i < lines; ++i)
            offsets[i] = static_cast<size_t>(slots[i]) * kSlotBytes +
                (static_cast<size_t>(i) * stride) % kSlotBytes;
        const uint64_t control = build_ring_at(base, order, offsets, line);
        const uint64_t test = build_ring_at(base, order, offsets, 0);
        const double ratio = conflict_ratio(base, test, control);
        if (debug)
            std::fprintf(stderr,
                "way-size probe [%s]: lines=%d stride=%zu ratio=%.3f\n",
                placement, lines, stride, ratio);
        return ratio >= kConflictRatio;
    };
    for (size_t stride = std::max<size_t>(line * 2, 1024); stride <= kSlotBytes;
        stride *= 2) {
        if (!conflicts(stride)) continue;
        if (stride == kSlotBytes || conflicts(stride * 2)) return stride;
    }
    return 0;
}

} // namespace

int probe_l1_line_from_sets(int ways, size_t way_bytes)
{
    if (ways <= 0 || way_bytes == 0) return 0;
    const int lines = std::min(2 * ways, kMaxLines);
    const size_t bytes = kSlotBytes * kSlotCount + kSlotBytes;
    void *allocation = nullptr;
    if (posix_memalign(&allocation, kSlotBytes, bytes) != 0) return 0;
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
    (void)madvise(allocation, bytes, MADV_NOHUGEPAGE);
#endif
    uint64_t *base = static_cast<uint64_t *>(allocation);
    const bool debug = debug_enabled();
    std::vector<int> order(lines);
    for (int i = 0; i < lines; ++i) order[i] = i;
    std::mt19937 generator(0x9e3779b9U + lines);
    std::shuffle(order.begin(), order.end(), generator);

    // Smallest shift that splits the set, for one placement.  A way
    // predictor collision can only keep a conflict alive longer, so across
    // placements the smallest answer is the line.
    auto line_for = [&](const std::vector<int> &slots, const char *placement) {
        for (size_t shift = 16; shift <= 512; shift *= 2) {
            std::vector<size_t> offsets(lines);
            for (int i = 0; i < lines; ++i)
                offsets[i] = static_cast<size_t>(slots[i]) * kSlotBytes +
                    (i % 2 ? shift : 0);
            // Control: every line in a set of its own, 4 KiB-aligned skews
            // keep them clear of the test lines' two sets.
            const uint64_t control =
                build_ring_at(base, order, offsets, way_bytes / 8);
            const uint64_t test = build_ring_at(base, order, offsets, 0);
            const double ratio = conflict_ratio(base, test, control);
            if (debug)
                std::fprintf(stderr,
                    "line-from-sets probe [%s]: shift=%zu ratio=%.3f\n",
                    placement, shift, ratio);
            if (ratio < kConflictRatio) return static_cast<int>(shift);
        }
        return 0;
    };
    int detected = 0;
    std::vector<int> slots(lines);
    for (int i = 0; i < lines; ++i) slots[i] = i * 16;
    detected = line_for(slots, "regular");
    const unsigned seeds[] = {0x43505546U, 0x9e3779b9U, 0x7f4a7c15U};
    for (unsigned seed : seeds) {
        std::vector<int> shuffled(kSlotCount);
        for (int i = 0; i < kSlotCount; ++i) shuffled[i] = i;
        std::mt19937 slot_generator(seed);
        std::shuffle(shuffled.begin(), shuffled.end(), slot_generator);
        shuffled.resize(lines);
        const int line = line_for(shuffled, "random");
        if (line > 0 && (detected == 0 || line < detected)) detected = line;
    }
    std::free(allocation);
    return detected;
}

size_t probe_l1_way_bytes(int cacheline_bytes, int ways)
{
    if (ways <= 0) return 0;
    const size_t line = cacheline_bytes > 0 ? cacheline_bytes : 64;
    // Twice the ways: one set cannot hold them, two sets hold exactly `ways`
    // each, which any replacement policy keeps without a miss.
    const int lines = std::min(2 * ways, kMaxLines);
    const size_t bytes = kSlotBytes * kSlotCount + kSlotBytes;
    void *allocation = nullptr;
    if (posix_memalign(&allocation, kSlotBytes, bytes) != 0) return 0;
#if defined(__linux__) && defined(MADV_NOHUGEPAGE)
    (void)madvise(allocation, bytes, MADV_NOHUGEPAGE);
#endif
    uint64_t *base = static_cast<uint64_t *>(allocation);
    const bool debug = debug_enabled();

    // As with the associativity, an address-hashed way predictor can make a
    // placement conflict early and never late, so the largest answer over
    // several placements is the one without such a collision.
    size_t detected = 0;
    std::vector<int> slots(lines);
    for (int i = 0; i < lines; ++i) slots[i] = i * 16;
    detected = std::max(detected,
        way_bytes_for_placement(base, slots, line, lines, "regular", debug));
    const unsigned seeds[] = {0x43505546U, 0x9e3779b9U, 0x7f4a7c15U};
    for (unsigned seed : seeds) {
        std::vector<int> shuffled(kSlotCount);
        for (int i = 0; i < kSlotCount; ++i) shuffled[i] = i;
        std::mt19937 slot_generator(seed);
        std::shuffle(shuffled.begin(), shuffled.end(), slot_generator);
        shuffled.resize(lines);
        detected = std::max(detected,
            way_bytes_for_placement(
                base, shuffled, line, lines, "random", debug));
    }
    std::free(allocation);
    return detected;
}

int probe_l2_associativity(int cacheline_bytes, int l1_ways)
{
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    if (l1_ways <= 0) return 0;
    std::ifstream thp("/sys/kernel/mm/transparent_hugepage/enabled");
    std::string policy;
    if (!thp || !std::getline(thp, policy) ||
        policy.find("[never]") != std::string::npos)
        return 0;
    const size_t line = cacheline_bytes > 0 ? cacheline_bytes : 64;
    const size_t huge_bytes = 2ULL * 1024 * 1024;
    const size_t bytes = huge_bytes * (kMaxLines + 1);
    void *allocation = nullptr;
    if (posix_memalign(&allocation, huge_bytes, bytes) != 0) return 0;
    (void)madvise(allocation, bytes, MADV_HUGEPAGE);
    // Touch one line per page so each is faulted in as a huge page now,
    // rather than as small pages when the rings are written.
    for (size_t page = 0; page <= static_cast<size_t>(kMaxLines); ++page)
        static_cast<volatile char *>(allocation)[page * huge_bytes] = 0;
    uint64_t *base = static_cast<uint64_t *>(allocation);
    const bool debug = debug_enabled();

    std::vector<size_t> offsets(kMaxLines);
    for (int i = 0; i < kMaxLines; ++i)
        offsets[i] = static_cast<size_t>(i) * huge_bytes;
    double l2_level = 0.0; // test/control ratio while hitting in L2
    int pending = 0;
    int result = 0;
    for (int lines = l1_ways + 2; lines <= kMaxLines; ++lines) {
        std::vector<int> order(lines);
        for (int i = 0; i < lines; ++i) order[i] = i;
        std::mt19937 generator(0x9e3779b9U + lines);
        std::shuffle(order.begin(), order.end(), generator);
        const uint64_t control = build_ring_at(base, order, offsets, line);
        const uint64_t test = build_ring_at(base, order, offsets, 0);
        const double ratio = conflict_ratio(base, test, control);
        if (debug)
            std::fprintf(stderr,
                "L2 associativity probe: lines=%d ratio=%.3f\n", lines, ratio);
        // Just past the L1 ways every access is an L2 hit; that ratio is the
        // level the second transition is measured against.
        if (l2_level == 0.0) {
            l2_level = ratio;
            if (ratio < kConflictRatio) break; // no L1 conflict: no targeting
            continue;
        }
        // Leaving L2 costs at least what entering it did, so 1.5 times the L2
        // level is well clear of its noise.
        if (ratio >= 1.5 * l2_level) {
            if (pending != 0) {
                result = pending - 1;
                break;
            }
            pending = lines;
        } else {
            pending = 0;
        }
    }
    std::free(allocation);
    return result;
#else
    (void)cacheline_bytes;
    (void)l1_ways;
    return 0;
#endif
}

int probe_l2_line_from_sets(int l2_ways)
{
#if defined(__linux__) && defined(MADV_HUGEPAGE)
    if (l2_ways <= 0) return 0;
    const int lines = std::min(2 * l2_ways, kMaxLines);
    const size_t huge_bytes = 2ULL * 1024 * 1024;
    const size_t bytes = huge_bytes * (lines + 1);
    void *allocation = nullptr;
    if (posix_memalign(&allocation, huge_bytes, bytes) != 0) return 0;
    (void)madvise(allocation, bytes, MADV_HUGEPAGE);
    for (int page = 0; page <= lines; ++page)
        static_cast<volatile char *>(allocation)[page * huge_bytes] = 0;
    uint64_t *base = static_cast<uint64_t *>(allocation);
    const bool debug = debug_enabled();
    std::vector<int> order(lines);
    for (int i = 0; i < lines; ++i) order[i] = i;
    std::mt19937 generator(0x9e3779b9U + lines);
    std::shuffle(order.begin(), order.end(), generator);

    // The first shift is inside any line, so its ratio is the L2-thrashing
    // level; the line is the first shift that brings the ratio well below it.
    double thrashing = 0.0;
    int result = 0;
    for (size_t shift = 32; shift <= 1024; shift *= 2) {
        std::vector<size_t> offsets(lines);
        for (int i = 0; i < lines; ++i)
            offsets[i] =
                static_cast<size_t>(i) * huge_bytes + (i % 2 ? shift : 0);
        const uint64_t control = build_ring_at(base, order, offsets, 4096);
        const uint64_t test = build_ring_at(base, order, offsets, 0);
        const double ratio = conflict_ratio(base, test, control);
        if (debug)
            std::fprintf(stderr,
                "L2 line-from-sets probe: shift=%zu ratio=%.3f\n", shift,
                ratio);
        if (thrashing == 0.0) {
            thrashing = ratio;
            continue;
        }
        if (ratio * 1.5 < thrashing) {
            result = static_cast<int>(shift);
            break;
        }
    }
    std::free(allocation);
    return result;
#else
    (void)l2_ways;
    return 0;
#endif
}

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
