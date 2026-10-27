#ifndef CPUFB_MEMORY_BANDWIDTH_HPP
#define CPUFB_MEMORY_BANDWIDTH_HPP

#include <cstdint>
#include <string>

#include "sys/calling_convention.hpp"
#include "cli.hpp"

namespace cpufb {

// Sequential-read kernel: (data, blocks per pass, passes).
typedef void (CPUFB_SYSV_ABI *StreamKernel)(float *, int, int64_t);

struct StreamKernelSpec
{
    StreamKernel function;
    std::string name;
    std::uint64_t bytes_per_block;
    std::uint64_t loads_per_block;
};

// The only architecture-specific part of the stream benchmark: the widest
// load kernel the running CPU supports.  Implemented in
// <arch>/kernel/stream_kernel.cpp.
StreamKernelSpec select_stream_kernel();

// Store streams for the write rows of the cache table, same signature and
// block convention as the read kernel.  The partial-line kernel writes 16 B
// of every 64 B line and counts the whole lines it touches as its bytes, so
// its GB/s compares directly with the full-line stream (see
// describe_write_allocate).  `function` is nullptr where an architecture has
// no such kernel.
StreamKernelSpec select_store_kernel();
StreamKernelSpec select_partial_store_kernel();
StreamKernelSpec select_nontemporal_store_kernel();

// Running clock of the calling thread's core from the architecture's ADD
// dependency chain (one dependent register-register ADD per cycle), or 0 when
// the architecture has none.  Also implemented in stream_kernel.cpp.
double estimate_core_clock_hz();

} // namespace cpufb

// --memory-bandwidth: one sequential-read stream per selected CPU.
bool run_memory_bandwidth(const cpufb::cli::CliOptions &options);

// L3 and DRAM read stream rows of the cache table, then the DRAM write
// streams (full-line, partial-line, non-temporal) and the write-allocate
// verdict.  Store rows are skipped when the architecture has no store kernel.
// measured_last_level_bytes: the deepest level the latency curve resolved on
// the first core, or 0; the L3 stream is kept inside it (see
// cap_to_measured_level).
bool append_cache_memory_bandwidth(const cpufb::cli::CliOptions &options,
    Table &table, std::uint64_t measured_last_level_bytes = 0);

#endif
