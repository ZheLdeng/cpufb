#ifndef CPUFB_CACHELINE_PROBE_HPP
#define CPUFB_CACHELINE_PROBE_HPP

#include <cstddef>

namespace cpufb {

typedef void (*cacheline_flush_fn)(void *address);
typedef void (*cacheline_fence_fn)();

// Returns the empirically measured cache-line size in bytes, or 0 when no
// boundary was observed.  theory_cacheline is only used for debug output; the
// measurement never depends on, and is never replaced by, the OS value.
// last_level_cache_bytes sizes the eviction buffer; pass the largest capacity
// found by the latency curve (a measurement), or 0 when unknown.
int probe_cacheline_size(int theory_cacheline, cacheline_flush_fn flush_line,
    cacheline_fence_fn finish_flush, size_t last_level_cache_bytes = 0);

// Line size other probes should build on: the OS value when exposed, then
// the measurement, then the architecture fallback.
int effective_cacheline_size(
    int theory_cacheline, int measured_cacheline, int fallback_cacheline);

} // namespace cpufb

#endif
