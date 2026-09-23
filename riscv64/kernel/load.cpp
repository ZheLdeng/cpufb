#include "load.hpp"

#include "associativity_probe.hpp"
#include "cacheline_probe.hpp"

#include <cstddef>

// This file used to hold a second cache-size estimator (a slope rule over a
// hand-rolled pointer walk) and an associativity walk, neither of which was
// ever called: riscv64/cpufb.cpp has always used the shared latency curve.
// The estimator also read /cache/index0 as the L1 data cache, which is the
// instruction cache on a SpacemiT X60, and carried a copied __APPLE__ branch.
// What the backend was missing instead were the two probes below.

namespace {

// The line-size probe needs its node pairs cold before each timed walk.  It
// gets there two ways, a cache-maintenance instruction and an eviction buffer
// larger than the last level, and either one alone is enough.  RISC-V has
// cbo.flush, but it is an extension (Zicbom) and whether it traps in user
// mode is up to the kernel's senvcfg, so a machine without it would take an
// illegal-instruction trap here.  This backend therefore leaves the flush
// empty and lets capacity eviction do the work.  That is the path
// CPUFB_CACHELINE_NO_FLUSH exercises on the other two architectures, where it
// reads the right line size on its own: x86-64 rises 0.045 to 0.93 across the
// boundary, a Kunpeng 920F 0.064 to 0.41.
void flush_cache_line(void *) {}

void finish_cache_line_flush()
{
    __asm__ __volatile__("fence rw, rw" ::: "memory");
}

} // namespace

CacheGeometryProbe probe_cache_geometry(
    int reported_cacheline, uint64_t last_level_bytes)
{
    CacheGeometryProbe result;
    result.cacheline_bytes =
        cpufb::probe_cacheline_size(reported_cacheline, flush_cache_line,
            finish_cache_line_flush, static_cast<size_t>(last_level_bytes));
    // The associativity ring needs a line size to step by; the probe's own
    // answer is preferred, and the OS value is only a fallback for a machine
    // where the line probe found no boundary.
    const int line = cpufb::effective_cacheline_size(
        0, result.cacheline_bytes, reported_cacheline);
    result.l1_ways = cpufb::probe_l1_associativity(line);
    result.l1_way_bytes = cpufb::probe_l1_way_bytes(line, result.l1_ways);
    result.line_from_sets =
        cpufb::probe_l1_line_from_sets(result.l1_ways, result.l1_way_bytes);
    result.l2_ways = cpufb::probe_l2_associativity(line, result.l1_ways);
    result.l2_line = cpufb::probe_l2_line_from_sets(result.l2_ways);
    return result;
}
