#ifndef CPUFB_MEMORY_BANDWIDTH_HPP
#define CPUFB_MEMORY_BANDWIDTH_HPP

#include <cstdint>
#include <string>

#include "cli.hpp"

namespace cpufb {

// Sequential-read kernel: (data, blocks per pass, passes).
typedef void (*StreamKernel)(float *, int, int64_t);

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

// Running clock of the calling thread's core from the architecture's ADD
// dependency chain (one dependent register-register ADD per cycle), or 0 when
// the architecture has none.  Also implemented in stream_kernel.cpp.
double estimate_core_clock_hz();

} // namespace cpufb

// --memory-bandwidth: one sequential-read stream per selected CPU.
bool run_memory_bandwidth(const cpufb::cli::CliOptions &options);

// L3 and DRAM stream rows of the cache table.
// measured_last_level_bytes: the deepest level the latency curve resolved on
// the first core, or 0; the L3 stream is kept inside it (see
// cap_to_measured_level).
bool append_cache_memory_bandwidth(const cpufb::cli::CliOptions &options,
    Table &table, std::uint64_t measured_last_level_bytes = 0);

#endif
