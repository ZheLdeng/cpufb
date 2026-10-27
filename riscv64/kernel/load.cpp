#include "load.hpp"

#include "compute.hpp"

// The pointer-chase and line-flush kernels of this architecture.  The cache
// geometry itself (reported values, the latency curve, the line and
// set-conflict probes and the table rows) is common/cache/cache_report.cpp.

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

cpufb::CacheProbeKernels cache_probe_kernels()
{
    cpufb::CacheProbeKernels kernels;
#ifdef _VECTOR_
    kernels.chase = riscv_cache_chase32;
#endif
    kernels.flush = flush_cache_line;
    kernels.fence = finish_cache_line_flush;
    // 64 MiB: the SpacemiT X60 has no L3 and its memory is slow.
    kernels.curve_max_bytes = 64ULL << 20;
    return kernels;
}
