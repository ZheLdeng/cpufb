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
    // Latency at the end of the plateau this level sits on, over the latency
    // at its start.  1.0 is a flat plateau; an Apple M4 Pro's cluster-shared
    // L2 climbs by 2-3x across its own range before its boundary, and that
    // is what makes its capacity land far above the start of its rise.
    double plateau_drift = 1.0;
};

// A sample measured on three independent rings because it decides a result
// (see measure_cache_curve); the point keeps the median.
struct RemeasuredPoint
{
    uint64_t working_set_bytes = 0;
    double samples_ns[3] = {0.0, 0.0, 0.0};
};

struct CacheCurveResult
{
    std::vector<CacheLatencyPoint> points;
    std::vector<RemeasuredPoint> remeasured;
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

// How far the latency climbed before it settled, for a level whose rise was
// wider than one working-set doubling; empty for a clean step.  This is an
// annotation, not a verdict: two rounds of hardware reports showed that the
// width of a rise, and where the capacity sits inside it, both fail to tell
// a real boundary from a prefetched plateau.  See describe_prefetch_doubt()
// for the signal that does carry information.
std::string describe_transition(const CacheLevelEstimate &level);
const double kWideRiseWidth = 2.5;

// Set when the deepest working set in the sweep is still served faster than
// memory: the chase was prefetched everywhere, so no capacity read from this
// curve can be trusted.  Empty otherwise.
std::string describe_prefetch_doubt(const CacheCurveResult &result);

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
