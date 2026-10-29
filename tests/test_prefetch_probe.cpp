// Unit tests for the analysis half of the prefetch probe and for the
// write-allocate verdict: the rules that turn measured latencies into
// verdicts, on synthetic results.

#include "cache/cache_topology.hpp"
#include "cache/prefetch_probe.hpp"

#include <cmath>
#include <iostream>
#include <string>

using cpufb::PrefetchDistance;
using cpufb::PrefetchProbeResult;
using cpufb::PrefetchSegment;
using cpufb::PrefetchStride;

namespace {

bool expect(bool condition, const char *name)
{
    if (!condition) std::cerr << "FAILED: " << name << '\n';
    return condition;
}

bool near(double value, double expected, double tolerance)
{
    return std::fabs(value - expected) <= tolerance;
}

// A core whose L2 prefetcher follows strides up to 4 lines and whose memory
// prefetcher only follows stride 1 and 2.
PrefetchProbeResult synthetic()
{
    PrefetchProbeResult result;
    result.worksets[0].bytes = 256 << 10;
    result.worksets[0].random_ns = 4.0;
    result.worksets[1].bytes = 64ULL << 20;
    result.worksets[1].random_ns = 100.0;
    const size_t strides[] = {1, 2, 4, 8, 16, 32, 64};
    const double l2[] = {1.6, 1.7, 2.6, 3.9, 4.0, 4.0, 4.0};
    const double memory[] = {6.0, 30.0, 70.0, 95.0, 100.0, 100.0, 100.0};
    for (size_t i = 0; i < 7; ++i) {
        PrefetchStride row;
        row.stride_lines = strides[i];
        row.ns[0] = l2[i];
        row.ns[1] = memory[i];
        result.strides.push_back(row);
    }
    // T(S) = 6 + 180 / S exactly.
    const size_t segments[] = {4, 8, 16, 32, 64, 128, 256, 1024};
    for (size_t s : segments) {
        PrefetchSegment row;
        row.lines = s;
        row.ns = 6.0 + 180.0 / s;
        result.segments.push_back(row);
    }
    // Memory: 100 ns alone, floor 6.25 ns from distance 16 (16 in flight);
    // L2 workset: software prefetch changes nothing.
    const size_t distances[] = {0, 1, 2, 4, 8, 16, 32, 64};
    const double memory_l1[] = {100, 52, 27, 14.5, 8.0, 6.3, 6.25, 6.4};
    for (size_t i = 0; i < 8; ++i) {
        PrefetchDistance row;
        row.distance = distances[i];
        row.ns[0] = 4.0 - 0.05 * i;
        row.ns[1] = memory_l1[i];
        row.ns[2] = memory_l1[i] * 1.2;
        result.distances.push_back(row);
    }
    return result;
}

} // namespace

int main()
{
    bool ok = true;
    const PrefetchProbeResult result = synthetic();

    // Stride reach: the L2 prefetcher hides 60% at stride 1 (2.4 ns of 4),
    // still 1.4 ns (> 1.2) at stride 4 and only 0.1 ns at stride 8.
    const cpufb::StrideReach l2 =
        cpufb::stride_reach(result, cpufb::PREFETCH_L2_WORKSET);
    ok &= expect(l2.reach_lines == 4, "L2 reach is 4 lines");
    ok &= expect(near(l2.hidden_stride1, 0.6, 1e-9), "L2 hidden share at stride 1");
    const cpufb::StrideReach memory =
        cpufb::stride_reach(result, cpufb::PREFETCH_MEMORY_WORKSET);
    ok &= expect(memory.reach_lines == 2, "memory reach is 2 lines");

    // No prefetcher at all: stride 1 as slow as random order.
    PrefetchProbeResult flat = result;
    for (PrefetchStride &row : flat.strides) row.ns[1] = 99.0;
    ok &= expect(
        cpufb::stride_reach(flat, cpufb::PREFETCH_MEMORY_WORKSET).reach_lines == 0,
        "no sequential prefetch observed");
    // A stride that is followed again after one that was not does not extend
    // the reach (noise on a VM did exactly that).
    PrefetchProbeResult gap = result;
    gap.strides[2].ns[1] = 99.0;
    ok &= expect(
        cpufb::stride_reach(gap, cpufb::PREFETCH_MEMORY_WORKSET).reach_lines == 2,
        "reach stops at the first stride not followed");

    // Restart cost: the fit recovers C = 180 ns on exact data; 10% of a
    // 6 ns steady stream needs 300 lines, so the first measured segment
    // within it is 1024.  A core that never gets within 10% reports none and
    // the overhead of its longest segment.
    const cpufb::RestartCost cost = cpufb::fit_restart_cost(result.segments, 6.0);
    ok &= expect(near(cost.cost_ns, 180.0, 1e-6), "restart cost fit");
    ok &= expect(cost.lines_for_ten_percent == 1024, "lines for ten percent");
    ok &= expect(cost.longest_lines == 1024 &&
            near(cost.longest_overhead, 180.0 / 1024 / 6.0, 1e-9),
        "longest segment overhead");
    ok &= expect(cpufb::fit_restart_cost(result.segments, 5.0)
                     .lines_for_ten_percent == 0,
        "no segment within 10% of a 5 ns stream");
    ok &= expect(cpufb::fit_restart_cost(result.segments, 0.0).cost_ns == 0.0,
        "no steady rate, no cost");

    // Distances: memory column, best at 32 (6.25), within 3% from 16 on,
    // 16 lines in flight; the L2 column is not an effect (4.0 to 3.65).
    const cpufb::DistanceSummary mem = cpufb::summarize_distances(result, 1);
    ok &= expect(mem.effective, "memory prefetch is effective");
    ok &= expect(mem.best_distance == 32, "best distance 32");
    ok &= expect(mem.distance_needed == 16, "distance needed 16");
    ok &= expect(near(mem.lines_in_flight, 16.0, 1e-9), "16 lines in flight");
    const cpufb::DistanceSummary l2d = cpufb::summarize_distances(result, 0);
    ok &= expect(!l2d.effective, "L2 workset prefetch is not an effect");
    ok &= expect(cpufb::summarize_distances(result, 7).alone_ns == 0.0,
        "unknown column is empty");

    // Write allocate: equal lines per second means the full-line stream
    // paid the same fill; twice as many lines per second means it did not.
    ok &= expect(cpufb::describe_write_allocate(10.0, 9.5, 0.0).find(
                     "gain nothing over partial-line") != std::string::npos,
        "write allocate at 1.05x");
    ok &= expect(cpufb::describe_write_allocate(19.0, 10.0, 25.0).find(
                     "skip the line fill") != std::string::npos,
        "no write allocate at 1.9x");
    ok &= expect(cpufb::describe_write_allocate(19.0, 10.0, 25.0).find(
                     "non-temporal stores 1.32x") != std::string::npos,
        "non-temporal ratio reported");
    ok &= expect(cpufb::describe_write_allocate(14.0, 10.0, 0.0).find(
                     "inconclusive") != std::string::npos,
        "inconclusive between the thresholds");
    ok &= expect(cpufb::describe_write_allocate(6.6, 10.0, 26.0).find(
                     "gain nothing over partial-line") != std::string::npos,
        "a slower full-line stream gets the same verdict");
    ok &= expect(cpufb::describe_write_allocate(0.0, 10.0, 0.0).find(
                     "not measured") != std::string::npos,
        "a failed stream is not measured");

    if (ok) std::cout << "prefetch probe analysis: all checks passed\n";
    return ok ? 0 : 1;
}
