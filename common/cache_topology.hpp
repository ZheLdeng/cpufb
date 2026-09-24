#ifndef CPUFB_CACHE_TOPOLOGY_HPP
#define CPUFB_CACHE_TOPOLOGY_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace cpufb {

// A data or unified cache at a requested hierarchy level. Linux reports this
// directly through sysfs; macOS exposes available L1D/L2/L3 capacities
// through sysctl.
struct CacheLevelInfo
{
    std::uint64_t bytes = 0;
    int level = 0;
    std::string source;
    // Linux sysfs only; 0 where the OS does not say.  shared_cpus is how many
    // CPUs the OS lists as sharing this cache: for a last level shared by a
    // whole chip, `bytes` is that chip's total, not what one core reaches.
    int line_bytes = 0;
    int ways = 0;
    int shared_cpus = 0;
};

CacheLevelInfo detect_data_cache_level(int cpu, int level);

// The largest data/unified cache exposed by the operating system for one CPU.
// On Apple Silicon, macOS does not expose the system-level cache through a
// public sysctl, so this can be the outermost per-performance-level L2 cache.
struct LastLevelCacheInfo
{
    std::uint64_t bytes = 0;
    int level = 0;
    bool is_reported_llc = false;
    std::string source;
};

LastLevelCacheInfo detect_last_level_cache(int cpu);

// A sequential memory stream should be materially larger than the last-level
// cache, but should not allocate 1 GiB by default on every machine.
std::uint64_t recommended_stream_workset_bytes(const LastLevelCacheInfo &cache);

std::string format_cache_capacity(std::uint64_t bytes);

// Probe cell of the L3 row.  The latency curve either found a third level,
// or climbed from L2 straight to memory latency (no L3: Kunpeng 920F, Apple
// Silicon), or never reached memory latency, in which case it cannot tell.
std::string format_probed_l3(int l3_kib, bool hierarchy_complete);

// One-phrase verdict for a probe result next to the OS-reported value.  The
// probe value is always printed as measured; this only labels agreement.
// tolerance is the largest measured/reported ratio, in either direction, that
// counts as agreement: 1.0 for discrete values and 1.3 for capacities, which
// the sweep samples on a quarter-octave grid (one step is at most 1.25x).
// The ratio is part of the label whenever it is not exactly 1.
std::string describe_probe_agreement(
    double reported, double measured, double tolerance);

// L1 capacity measured from set conflicts, ways x bytes per way (see
// probe_l1_way_bytes).  The verdict names both factors.
std::string describe_l1_geometry(
    std::uint64_t reported_bytes, int ways, std::size_t way_bytes);

// Note for the latency curve's L1 row when set conflicts place the L1
// elsewhere: the curve measures how far a pointer chase stays at L1 latency,
// which a prefetcher can stretch, and the conflicts measure the cache.
// Empty when they agree within a quarter-octave step or either is missing.
std::string describe_l1_curve_check(
    int curve_kib, int ways, std::size_t way_bytes);

// Note for a probed level smaller than an OS value that several CPUs share:
// the OS reports the whole cache, the probe what one core reaches.
std::string describe_shared_level(const CacheLevelInfo &os, int probe_kib);

// Note for the L2 line row when the OS reports a different line size for the
// L3.  L1 and L2 lines are read from set indexing; a last level shared by a
// chip is hashed across slices, so no set of it can be targeted, and a line
// read from reuse timing cannot be told apart from a shorter line whose
// neighbour is fetched with it.  Empty when the L3 line matches.
std::string describe_deeper_line_sizes(int cpu, int l2_line_bytes);

// Cross-check of the reuse-based line probe against the set-index granule
// (probe_l1_line_from_sets).  The granule is a lower bound on the line, the
// reuse reading an upper bound; when they differ the note states the bound.
std::string describe_line_cross_check(int reuse_line_bytes, int set_line_bytes);

// Verdict for an L2 ways or line row the set-conflict probe could not fill:
// status is the probe's return value (negative: no huge pages; 0: huge
// pages, but lines placed in one set by address never conflicted).
std::string describe_l2_unmeasured(int status);

} // namespace cpufb

#endif
