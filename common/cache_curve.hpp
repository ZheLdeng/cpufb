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

// Follows `iterations` links of the ring stored in buffer, starting at word 0.
typedef void (*CacheChaseKernel)(int iterations, int64_t *buffer);

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
