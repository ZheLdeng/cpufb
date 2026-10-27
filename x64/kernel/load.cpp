#include "load.hpp"

#include <immintrin.h>

// The pointer-chase and line-flush kernels of this architecture.  The cache
// geometry itself (reported values, the latency curve, the line and
// set-conflict probes and the table rows) is common/cache/cache_report.cpp.

namespace {

void flush_cache_line(void *addr)
{
    _mm_clflush(addr);
}

void finish_cache_line_flush()
{
    _mm_mfence();
}

// Dependent-load ring walk; the empty asm keeps `next` live so the chain is
// neither vectorized nor dropped.
void chase_ring(int iterations, const int32_t *buffer)
{
    int32_t next = 0;
    for (int i = 0; i < iterations; ++i) next = buffer[next];
    __asm__ volatile("" : "+r"(next) : : "memory");
}

} // namespace

cpufb::CacheProbeKernels cache_probe_kernels()
{
    cpufb::CacheProbeKernels kernels;
    kernels.chase = chase_ring;
    kernels.flush = flush_cache_line;
    kernels.fence = finish_cache_line_flush;
    return kernels;
}
