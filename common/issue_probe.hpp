#ifndef CPUFB_ISSUE_PROBE_HPP
#define CPUFB_ISSUE_PROBE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "table.hpp"

// Issue-rate probe: how many instructions of one class a core sustains per
// cycle, and how two or three classes share the core when mixed.
//
// The kernels are generated at build time from <arch>/kernel/issue_classes.def
// (see issue_layout.h for the grid).  Every kernel is a straight-line stream
// of independent instructions: each class rotates over its own registers so
// that no instruction waits for another, and loop control is under one
// percent of the body.  Cycles are counted with the hardware counter around
// the kernel; when that is unavailable they are elapsed time times a clock.
//
// For a pair (A, B) the probe measures the total IPC T(x) of a stream whose
// fraction of class-A instructions is x, on the Farey grid of small-integer
// ratios.  T is unimodal in x on every core measured: to the left of its
// maximum the stream is B-bound, to the right A-bound.  The maximum is found
// with a golden-section (Fibonacci) search, which needs O(log n) kernel runs
// instead of the whole grid, and the near-optimal interval is the set of
// ratios whose IPC is within epsilon of the maximum, found by bisection on
// either side.  The maximum T* against the pure peaks P_A and P_B tells how
// the classes share the core: T* ~ max(P_A, P_B) means one shared set of
// issue ports, T* ~ P_A + P_B means independent ones, and anything between is
// a shared budget of P_A + P_B - T* instructions per cycle.

namespace cpufb {

typedef void (*IssueKernelFn)(const void *l1_buffer, int64_t loops);

struct IssueClass
{
    const char *name;        // token used in symbols and mix definitions
    const char *instruction; // display form of the instruction
    const char *unit;        // "ALU", "FSU", "LSU", "SME"
    int bank;                // ISSUE_BANK_*
    int mode;                // ISSUE_MODE_*
    const char *feature;     // runtime feature token, "" when always present
    IssueKernelFn pure_half; // rotating over half of the class's registers
    IssueKernelFn pure_full; // rotating over all of them
};

struct IssueMixKernel
{
    int counts[3];            // instructions of each class in one pattern
    IssueKernelFn interleaved; // classes evenly interleaved
    IssueKernelFn blocked;     // runs of one class, nullptr for triples
};

struct IssueMix
{
    const char *label;
    const char *classes[3]; // class names, nullptr when unused
    int class_count;
    const IssueMixKernel *kernels;
    size_t kernel_count;
};

struct IssueProbeInput
{
    const IssueClass *classes;
    size_t class_count;
    const IssueMix *mixes;
    size_t mix_count;
    int pure_body; // ISSUE_PURE_BODY
    bool (*feature_available)(const char *feature);
};

// ---------------------------------------------------------------------------
// Analysis, independent of any hardware (unit-tested in tests/).
// ---------------------------------------------------------------------------

struct IssueSearchResult
{
    size_t best;    // candidate with the largest value
    double value;   // that value
    size_t low;     // first candidate with value >= (1 - epsilon) * best
    size_t high;    // last such candidate
    int evaluations;
};

// Maximum of a unimodal function over candidates 0 .. count-1, ordered along
// the mixing fraction.  evaluate(i) is called at most once per candidate;
// two values within `tolerance` of each other count as equal, which keeps
// measurement noise from steering the search.  The interval [low, high]
// holds every candidate within `epsilon` of the maximum.
IssueSearchResult search_unimodal_maximum(size_t count,
    const std::function<double(size_t)> &evaluate, double tolerance,
    double epsilon);

enum IssueSharing { ISSUE_SHARED, ISSUE_INDEPENDENT, ISSUE_PARTIAL };

struct IssueVerdict
{
    IssueSharing sharing;
    double shared_budget; // P_A + P_B - T*, meaningful for ISSUE_PARTIAL
};

// How two classes share the core, from their pure peaks and the maximum
// total IPC of their mixes.
IssueVerdict classify_joint_ceiling(double peak_a, double peak_b, double joint);
std::string describe_issue_verdict(const IssueVerdict &verdict);

// Candidates whose instruction shares are nearest (L1 distance) to the
// peak-proportional mix, best first.
std::vector<size_t> nearest_proportional_mixes(
    const std::vector<std::vector<int>> &counts,
    const std::vector<double> &peaks, size_t take);

// Fraction of class A in a pattern of a A's and b B's.
inline double issue_mix_fraction(int a, int b)
{
    return static_cast<double>(a) / (a + b);
}

// ---------------------------------------------------------------------------
// Measurement.
// ---------------------------------------------------------------------------

struct IssueProbeOptions
{
    double sample_seconds; // target length of one timed sample
    int samples;           // samples per kernel, minimum kept
    double clock_hz;       // fallback clock when no cycle counter; 0 = none
    std::string clock_source;

    IssueProbeOptions();
};

// Runs the whole probe on `cpu` (the calling thread is pinned there) and
// appends its rows to `table`: one row per available class, one per mix, and
// a final row naming the cycle source.  The table has five columns: Item,
// Classes, IPC, Frontier, Verdict.
bool run_issue_probe(const IssueProbeInput &input, int cpu,
    const IssueProbeOptions &options, Table &table);

// The header row of that table.
void issue_probe_table_header(Table &table);

// The `multi_issue` category as every backend runs it: the frequency table's
// clock (GHz, 0 when none) and its source name are the fallback for hosts
// without a cycle counter, and loop_scale shortens the samples for smoke
// runs.
bool run_issue_probe_category(const IssueProbeInput &input, int cpu,
    double clock_ghz, const std::string &clock_source, unsigned loop_scale,
    Table &table);

} // namespace cpufb

#endif
