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
    // The same latency in core cycles counted by the PMU around the sample;
    // 0 when no counter was available (the caller may then use a clock).
    double latency_cycles = 0;
};

struct CacheLevelEstimate
{
    std::string level;
    uint64_t capacity_bytes = 0;
    double latency_ns = 0;
    double latency_cycles = 0; // 0 when the sweep had no cycle counter
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
    double samples_cycles[3] = {0.0, 0.0, 0.0};
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

// Core cycles around one timed sample when the PMU is accessible (a
// dependent-load latency in cycles is what a kernel's pipelining depth is
// sized from; nanoseconds depend on the clock the sweep ran at).  Shared by
// the latency curve and the prefetch probe.
class SampleCycles
{
public:
    SampleCycles();
    ~SampleCycles();
    SampleCycles(const SampleCycles &) = delete;
    SampleCycles &operator=(const SampleCycles &) = delete;
    bool available() const;
    void start();
    double stop(); // cycles counted since start(), 0 without a counter

private:
    void *counter_; // PerfEventCycle on Linux, null elsewhere
};

// An anonymous region aligned to 2 MiB, zero-filled, MADV_HUGEPAGE when
// transparent huge pages are enabled.
class MappedRegion
{
public:
    MappedRegion() : base_(nullptr), bytes_(0), mapping_(nullptr), mapping_bytes_(0) {}
    ~MappedRegion();
    MappedRegion(const MappedRegion &) = delete;
    MappedRegion &operator=(const MappedRegion &) = delete;
    bool map(uint64_t bytes, bool huge_pages);
    void *base() const { return base_; }
    uint64_t bytes() const { return bytes_; }

private:
    void *base_;
    uint64_t bytes_;
    void *mapping_;
    size_t mapping_bytes_;
};

bool transparent_huge_pages_available();

// MemAvailable (or the platform's equivalent), 0 when unknown.
uint64_t available_memory_bytes();

// Lines per group of the ring order for `line`-byte lines, and the name of
// the resulting translation mode: 0 (one global shuffle) with huge pages or
// base pages of 16 KiB and more, otherwise the lines of one page, so that
// translation misses stay amortized and cannot appear as a spurious level.
// CPUFB_CHASE_NO_PAGE_GROUPS=1 forces the global shuffle.
size_t ring_group_lines(size_t line, bool huge_pages, std::string &mode);

// Ring order over `line_count` lines.  group_lines == 0 shuffles globally;
// otherwise groups are visited in random order and shuffled internally.  The
// ring always starts at line 0 because the chase kernels start at word 0.
std::vector<size_t> build_ring_order(
    size_t line_count, size_t group_lines, uint64_t seed);

// "%.1f" of the counted cycles, else of latency_ns x fallback_clock_ghz,
// else "-".
std::string format_latency_cycles(
    double latency_ns, double counted_cycles, double fallback_clock_ghz);

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

// Prints the curve (working set, ns and cycles per load) and the level
// estimates as tables, after the translation mode and where the cycles come
// from: the PMU when it counted, else the latency times fallback_clock_ghz
// (the frequency table's clock), else nothing.
void print_cache_curve_tables(
    const CacheCurveResult &result, double fallback_clock_ghz);

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
