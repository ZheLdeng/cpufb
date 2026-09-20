#ifndef CPUFB_CACHE_BANDWIDTH_HPP
#define CPUFB_CACHE_BANDWIDTH_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "thread_pool.hpp"

// Sequential-read bandwidth of one cache level.
//
//   1. Pick a working set that lives in the level under test but not in the
//      level below it (cache_level_workset).  A sequential pass then re-reads
//      every line only after the whole set went by, so each read is served by
//      that level (and its prefetcher), never by the faster one.
//   2. Every selected core gets its own buffer, first touched and warmed by
//      the worker that is timed on it.
//   3. One sample moves about 1 GiB per core; the fastest of five is kept,
//      because interference can only slow a sample down.
//   4. GB/s needs nothing but the wall clock.  Byte/Cycle divides by core
//      cycles counted around the kernel (perf_event), or by elapsed time x a
//      caller-supplied clock when the PMU is not accessible.
//   5. Load IPC = Byte/Cycle / bytes per load instruction.  It tells what the
//      number means: a rate that stays at the core's load-issue limit from L1
//      to L2 is bounded by the core, not by the cache; a rate that drops is
//      the bandwidth of the level.

namespace cpufb {

// fn(data, count, passes): reads `count` units of `bytes_per_count` bytes
// starting at `data`, `passes` times over.
struct LoadKernel
{
    std::string name; // only used in diagnostics
    void (*function)(float *, int, int64_t) = nullptr;
    size_t bytes_per_count = 0; // bytes read per unit of `count`
    size_t block_bytes = 0;    // one unrolled loop body; worksets are multiples
    size_t bytes_per_load = 0; // bytes moved by one load instruction
};

struct CacheBandwidth
{
    double gb_per_second = 0.0;   // aggregate over all workers, 1 GB = 1e9 B
    double bytes_per_cycle = 0.0; // per core; 0 when no cycle count or clock
    double load_ipc = 0.0;        // load instructions per cycle, per core
    size_t workset_bytes = 0;     // per worker
    size_t worker_count = 0;
    std::string cycle_source; // "perf_event cycles", "time x clock" or ""
};

// Geometric middle of the window (lower level capacity, this level capacity]:
// far from both edges on a log scale.  Level 1 has nothing below it and uses
// half its capacity.  Returns 0 when the capacity is unknown.
size_t cache_level_workset(size_t lower_capacity_bytes, size_t capacity_bytes);

// Runs on the pinned workers of `pool`.  clock_hz is only used when PMU
// cycles are unavailable; pass 0 to leave Byte/Cycle empty in that case.
CacheBandwidth measure_cache_bandwidth(const LoadKernel &kernel,
    size_t workset_bytes, tpool_t *pool, double clock_hz);

} // namespace cpufb

#endif
