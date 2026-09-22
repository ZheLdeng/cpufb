#ifndef CPUFB_CACHE_CURVE_HPP
#define CPUFB_CACHE_CURVE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace cpufb {

// Dependent-load (pointer-chase) latency versus working-set size, and a
// capacity estimate for each cache level derived from that curve alone.
// Nothing here reads or accepts OS-reported cache sizes: the estimate has to
// stand as an independent measurement that can disagree with the topology.

struct CacheLatencyPoint
{
    uint64_t working_set_bytes = 0;
    double latency_ns = 0;
};

struct CacheLevelEstimate
{
    std::string level;
    uint64_t capacity_bytes = 0;
    double latency_ns = 0;
    double jump_ratio = 0;
    // Working sets at which the latency has covered 10% and 90% of the step,
    // log-interpolated between grid points, and their ratio.  How wide the
    // rise is does not by itself say whether the boundary is real: the ring
    // leaves a reuse-distance tail above every capacity (see
    // measure_pointer_chase), and an unresolved level above this one
    // stretches the rise further (this VM's L2 rises over 4.4x because its
    // shared L3 never forms a plateau), yet in both cases the latency still
    // jumps at the capacity.
    uint64_t rise_begin_bytes = 0;
    uint64_t rise_end_bytes = 0;
    double transition_width = 1.0;
    // What does say it is where capacity_bytes sits within that rise.  A
    // boundary makes the latency jump, so the threshold is crossed in the
    // same grid interval the rise starts in: every level measured on real
    // hardware so far sits at 0.95-1.01 of rise_begin_bytes.  A prefetcher
    // letting go gradually turns the step into a slope, the threshold is
    // crossed well up it, and capacity_bytes is an artefact of where the
    // threshold happens to lie (a MediaTek MT6993 big core: 1.38, reported
    // as a 192 KiB L1 for a rise spanning 139 KiB to 605 KiB).
    bool gradual = false;
};

struct CacheCurveResult
{
    std::vector<CacheLatencyPoint> points;
    std::vector<CacheLevelEstimate> levels;
    // Latency of a working set that no cache can hold (0 when not measured),
    // and whether the sweep climbed up to it.  When it did, `levels` is the
    // complete hierarchy: a machine without an L3 yields exactly L1 and L2.
    // When it did not, the last level is larger than the sweep.
    double memory_latency_ns = 0;
    bool reached_memory = false;
    // "huge pages", "page-grouped order" or "large base pages"; see
    // measure_cache_curve().
    std::string translation_mode;
};

// Follows `iterations` links of the ring stored in buffer, starting at index
// 0.  Links are 32-bit indices into the buffer (a 1 GiB region still fits), so
// a 64-byte line holds 16 of them.
typedef void (*CacheChaseKernel)(int iterations, const int32_t *buffer);

// Verdict text for a level whose capacity was withheld; empty for a level
// whose latency jumped at its capacity.
std::string describe_transition(const CacheLevelEstimate &level);
// Adjacent working sets on the quarter-octave grid differ by at most 1.25x,
// so a capacity more than this far above the start of the rise means a
// sample was already climbing before the threshold was crossed.
const double kGradualRisePosition = 1.25;

// With CPUFB_DEBUG_CACHE_CURVE set, writes the sampled curve and the level
// estimates to stderr; does nothing otherwise.  This is what a bug report
// about a wrong capacity needs to be diagnosed.
void debug_print_cache_curve(const CacheCurveResult &result);

// Quarter-octave working-set grid from 4 KiB to max_bytes.
std::vector<uint64_t> build_cache_curve_sizes(uint64_t max_bytes);

// Splits the curve into flat plateaus and places each capacity at the last
// working set that still performs like the lower of two adjacent plateaus
// (one third of the way up the step on a log scale).  Levels are named L1,
// L2, ... in order of appearance.  With a memory reference latency, plateaus
// at least half as slow as memory are memory rather than cache levels; that
// is what tells an L3 from DRAM on a machine that has no L3.
std::vector<CacheLevelEstimate> estimate_cache_levels(
    const std::vector<CacheLatencyPoint> &points,
    double memory_latency_ns = 0.0, bool *reached_memory = nullptr);

// Measures the curve on the calling thread (the caller pins it).  With
// transparent huge pages, or base pages of 16 KiB and more, the ring order is
// one global shuffle; with 4 KiB pages only, the order is shuffled page by
// page so that translation misses stay amortized and cannot appear as a
// spurious level between L1 and L2.
CacheCurveResult measure_cache_curve(
    CacheChaseKernel chase, int line_size, uint64_t max_bytes);

} // namespace cpufb

#endif
