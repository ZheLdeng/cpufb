#include "load.hpp"

// The pointer-chase and line-flush kernels of this architecture.  The cache
// geometry itself (reported values, the latency curve, the line and
// set-conflict probes and the table rows) is common/cache/cache_report.cpp.

extern "C"
{
    void load_ptr(int looptime, int64_t *ptr);
    void load_ptr32(int looptime, const int32_t *ptr);
}

namespace {

void flush_cache_line(void *address)
{
    asm volatile("dc civac, %0\n\t" // clean and invalidate cache line
        :
        : "r"(address)
        : "memory");
}

void finish_cache_line_flush()
{
    asm volatile("dsb ish\n\t"
                 "isb\n\t"
        :
        :
        : "memory");
}

} // namespace

cpufb::CacheProbeKernels cache_probe_kernels()
{
    cpufb::CacheProbeKernels kernels;
    kernels.chase = load_ptr32;
    kernels.flush = flush_cache_line;
    kernels.fence = finish_cache_line_flush;
#ifdef __APPLE__
    kernels.fallback_line_bytes = 128;
#endif
    return kernels;
}
