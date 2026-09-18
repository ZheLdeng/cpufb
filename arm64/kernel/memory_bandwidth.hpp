#ifndef CPUFB_ARM64_MEMORY_BANDWIDTH_HPP
#define CPUFB_ARM64_MEMORY_BANDWIDTH_HPP

#include "cli.hpp"

bool run_arm64_memory_bandwidth(const cpufb_cli::CliOptions &options);
bool append_arm64_cache_memory_bandwidth(
    const cpufb_cli::CliOptions &options, Table &table);

#endif
