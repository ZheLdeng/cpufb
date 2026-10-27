#ifndef CPUFB_RISCV64_LOAD_HPP
#define CPUFB_RISCV64_LOAD_HPP

#include "cache/cache_report.hpp"

using cpufb::CacheData;

// The chase kernel and line flush of this architecture (common/cache/cache_report).
cpufb::CacheProbeKernels cache_probe_kernels();

#endif
