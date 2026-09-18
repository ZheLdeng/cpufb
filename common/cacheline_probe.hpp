#ifndef CPUFB_CACHELINE_PROBE_HPP
#define CPUFB_CACHELINE_PROBE_HPP

typedef void (*cacheline_flush_fn)(void *address);
typedef void (*cacheline_fence_fn)();

int probe_cacheline_size(int theory_cacheline, int fallback_cacheline,
    cacheline_flush_fn flush_line,
    cacheline_fence_fn finish_flush);

#endif
