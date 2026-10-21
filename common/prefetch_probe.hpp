#ifndef CPUFB_PREFETCH_PROBE_HPP
#define CPUFB_PREFETCH_PROBE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cache_curve.hpp"

class Table;

namespace cpufb {

// What a kernel needs to know about prefetching, measured with dependent
// loads so that nothing but a prefetch can hide a miss.  Three sweeps share
// one buffer, one chase loop and one timing method (fastest of three
// samples, cycles counted around the same sample):
//
//  1. Hardware prefetcher reach.  The lines of a working set are chased in
//     sequential order with a stride of s lines, at an L2-resident working
//     set and at a memory one, against the same lines in random order.  The
//     share of the random latency that disappears is what the prefetcher
//     hid; the largest stride at which it still hides half of what it hides
//     at stride 1 is its reach.
//  2. Stream restart cost.  Sequential segments of S lines placed at random
//     in the memory working set.  The time per load is T + C/S, where T is the
//     steady-state stream rate (stride 1 above) and C the cost of every new
//     stream: the latency the prefetcher has not yet hidden and the lines it
//     fetched past the end of the previous segment.  C in line-times says
//     how long a panel must be before streaming it is worth anything.
//  3. Software prefetch distance.  The chain is prefetched D steps ahead
//     from a side array that holds the order, as a kernel prefetches from
//     addresses it knows in advance.  The time per load falls until the core
//     has no more miss slots, so the best time gives the misses it keeps in
//     flight and the first D within 3% of the best is the distance a kernel
//     should run ahead.  Measured for the L1 target at both working sets and
//     for the L2 target at the memory one.  Distance 0 is the plain chase
//     (the random-order latency of sweep 1), not a prefetch of the line
//     being loaded: the side array is read ahead of the chain by the
//     out-of-order core itself, so that would already be a prefetch.

enum PrefetchWorksetKind
{
    PREFETCH_L2_WORKSET = 0,
    PREFETCH_MEMORY_WORKSET = 1
};

struct PrefetchWorkset
{
    uint64_t bytes = 0; // 0 when the working set could not be formed
    double random_ns = 0;
    double random_cycles = 0; // 0 without a counter
};

struct PrefetchStride
{
    size_t stride_lines = 0;
    double ns[2] = {0, 0}; // per PrefetchWorksetKind; 0 when not measured
    double cycles[2] = {0, 0};
};

struct PrefetchSegment
{
    size_t lines = 0; // memory working set only
    double ns = 0;
    double cycles = 0;
};

// Which working set and which prefetch target a distance column measures.
struct PrefetchColumn
{
    PrefetchWorksetKind workset;
    bool l2_target; // false: nearest level (L1); true: the L2 target hint
};
const size_t kPrefetchColumns = 3;
extern const PrefetchColumn kPrefetchColumnSpec[kPrefetchColumns];
// The instruction the compiler emits for each target on this architecture.
const char *prefetch_instruction_name(bool l2_target);

struct PrefetchDistance
{
    size_t distance = 0;
    double ns[kPrefetchColumns] = {0, 0, 0};
    double cycles[kPrefetchColumns] = {0, 0, 0};
};

struct PrefetchProbeResult
{
    std::string translation_mode;
    PrefetchWorkset worksets[2];
    std::vector<PrefetchStride> strides;
    std::vector<PrefetchSegment> segments;
    std::vector<PrefetchDistance> distances;
};

// --- Analysis of a result (pure functions, unit-tested) --------------------

struct StrideReach
{
    size_t reach_lines = 0;  // 0: no sequential prefetch observed
    double hidden_stride1 = 0; // share of the random latency hidden at stride 1
};
// A stride is followed while it hides at least half of what stride 1 hides;
// the reach is the last stride before the first one not followed, and
// nothing is followed when stride 1 hides under 20%.
StrideReach stride_reach(const PrefetchProbeResult &result,
    PrefetchWorksetKind workset);

struct RestartCost
{
    double cost_ns = 0;   // C, the cost of starting a stream (fit)
    double steady_ns = 0; // T, the time per load of an endless stream
    // Shortest measured segment within 10% of T, 0 when none is; then the
    // longest segment measured and how far over T it still ran.
    size_t lines_for_ten_percent = 0;
    size_t longest_lines = 0;
    double longest_overhead = 0; // T(S)/T - 1 at the longest segment
};
// Least-squares fit of T + C/S through the segment points, T given; the
// 10% line is read off the points themselves, not the fit.
RestartCost fit_restart_cost(
    const std::vector<PrefetchSegment> &segments, double steady_ns);

struct DistanceSummary
{
    double alone_ns = 0;       // distance 0
    double best_ns = 0;
    size_t best_distance = 0;
    size_t distance_needed = 0; // first distance within 3% of the best
    double lines_in_flight = 0; // alone_ns / best_ns
    bool effective = false;     // best_ns <= 0.9 alone_ns
};
DistanceSummary summarize_distances(
    const PrefetchProbeResult &result, size_t column);

// --- Measurement and reporting ----------------------------------------------

// Runs on the calling thread (the caller pins it).  l2_bytes may be 0 (no
// L2-resident working set is then formed); the memory working set is
// max(64 MiB, 4 x deepest_bytes), limited by the available memory.
PrefetchProbeResult measure_prefetch_probe(
    int line_bytes, uint64_t l1_bytes, uint64_t l2_bytes, uint64_t deepest_bytes);

// The three sweeps as tables, after a header naming the working sets and the
// cycle source (as print_cache_curve_tables).
void print_prefetch_probe_tables(
    const PrefetchProbeResult &result, double fallback_clock_ghz);

// Runs the probe for the curve's levels, prints the tables and appends the
// verdict rows (stride reach, restart cost, one row per prefetch target) to
// the cache table.  A curve without levels yields one "not measured" row.
void append_prefetch_probe_rows(Table &table, int cpu, int line_bytes,
    const CacheCurveResult &curve, double fallback_clock_ghz);

} // namespace cpufb

#endif
