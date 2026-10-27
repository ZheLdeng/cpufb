#ifndef CPUFB_CACHE_REPORT_HPP
#define CPUFB_CACHE_REPORT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "cache/cache_curve.hpp"
#include "cache/cacheline_probe.hpp"

class Table;

namespace cpufb {

// The cache category of one core: what the OS reports and what the probes
// measured, side by side.  The probes never consume the reported values they
// are compared against (the line size is the one exception, and only as a
// step size), and a measurement that disagrees is printed and labelled, never
// replaced.  Shared by the three architectures; see measure_cache_report.
struct CacheData
{
    // Reported by the OS topology, 0 when it exposes nothing.  Capacities in
    // KiB; the sources name the interface the value came from.
    int theory_L1 = 0;
    int theory_L2 = 0;
    std::string theory_L1_source;
    std::string theory_L2_source;
    int theory_way = 0;
    int theory_l2_way = 0;
    int theory_cacheline = 0;
    int theory_l2_line = 0;

    // From the dependent-load latency curve (cache_curve.hpp): capacities in
    // KiB, test_L3 being the deepest level the curve resolved, and whether
    // the curve reached memory latency (so the level list is complete).
    CacheCurveResult curve;
    int test_L1 = 0;
    int test_L2 = 0;
    int test_L3 = 0;
    // Per level: empty for a clean step, the uncertainty range for a soft
    // one, the reason the capacity is withheld for a gradual one.
    std::string test_L1_note, test_L2_note, test_L3_note;
    double memory_latency_ns = 0;
    bool hierarchy_complete = false;
    // False when the curve fell or never reached memory; its capacities are
    // then not used to size anything else.
    bool curve_trusted = true;

    // From the line and set-conflict probes.  test_way x test_way_bytes is an
    // L1 capacity that does not come from the latency curve;
    // test_line_from_sets is the line size read from set indexing, immune to
    // neighbour prefetch.  0 means "not observed".
    int test_cacheline = 0;
    int test_way = 0;
    size_t test_way_bytes = 0;
    int test_line_from_sets = 0;
    int test_l2_way = 0;
    int test_l2_line = 0;
};

// What an architecture contributes: its pointer-chase kernel, its line
// flush (may be a no-op where the ISA has none usable from user mode) and the
// line size assumed when neither the OS nor the probe gives one.
struct CacheProbeKernels
{
    CacheChaseKernel chase = nullptr;
    cacheline_flush_fn flush = nullptr;
    cacheline_fence_fn fence = nullptr;
    int fallback_line_bytes = 64;
    // Sweep limit of the latency curve.  The plateau after the last cache
    // level needs three grid points, so 128 MiB covers an L3 of up to 64 MiB.
    uint64_t curve_max_bytes = 128ULL << 20;
};

// Fills the theory_* fields from the OS topology (sysfs, sysctl or Windows).
void read_reported_cache(CacheData &data, int cpu);

// The reported L1/L2 capacities with conservative fallbacks (64 KiB, 1 MiB)
// for the load table when nothing is reported and no probe has run yet.
void prepare_load_capacities(CacheData &data, int cpu);

// Runs the curve, then the line probe (its eviction buffer sized from the
// deepest level the curve found) and the set-conflict probes, on the calling
// thread pinned to `cpu`.  Fills every test_* field.
void measure_cache_report(
    CacheData &data, int cpu, const CacheProbeKernels &kernels);

// Appends the summary rows (capacities, ways, line sizes) to the cache
// table, prints the curve tables and appends the prefetch-probe rows.
void append_cache_report_rows(
    Table &table, const CacheData &data, int cpu, double fallback_clock_ghz);

// The deepest level the curve resolved, in bytes, when the curve is trusted;
// 0 otherwise.  This is what keeps the L3 stream inside the measured level.
uint64_t cache_report_last_level_bytes(const CacheData &data);

} // namespace cpufb

#endif
