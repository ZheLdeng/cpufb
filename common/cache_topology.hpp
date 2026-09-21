#ifndef CPUFB_CACHE_TOPOLOGY_HPP
#define CPUFB_CACHE_TOPOLOGY_HPP

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

// One-phrase verdict for a probe result next to the OS-reported value.  The
// probe value is always printed as measured; this only labels agreement.
// tolerance is the largest measured/reported ratio, in either direction, that
// counts as agreement: 1.0 for discrete values and 1.3 for capacities, which
// the sweep samples on a quarter-octave grid (one step is at most 1.25x).
// The ratio is part of the label whenever it is not exactly 1.
std::string describe_probe_agreement(
    double reported, double measured, double tolerance);

} // namespace cpufb

#endif
