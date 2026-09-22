#ifndef CPUFB_RISCV64_LOAD_HPP
#define CPUFB_RISCV64_LOAD_HPP

#include <cstdint>

// Line size and L1 associativity, both measured.  The reported line size is
// passed only so that the probe's debug output can show it next to its own
// answer; it never influences the measurement.  last_level_bytes sizes the
// eviction buffer and must come from the latency curve, not from the OS.
struct CacheGeometryProbe
{
    int cacheline_bytes = 0;
    int l1_ways = 0;
};

CacheGeometryProbe probe_cache_geometry(
    int reported_cacheline, uint64_t last_level_bytes);

#endif
