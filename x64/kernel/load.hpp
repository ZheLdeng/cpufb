#ifndef CPUFB_X64_LOAD_HPP
#define CPUFB_X64_LOAD_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cache/cache_report.hpp"
#include "sys/calling_convention.hpp"

extern std::vector<double> freq;
extern "C"
{
    void CPUFB_SYSV_ABI load_movups_kernel(float *, int64_t);
    void CPUFB_SYSV_ABI load_vmovups_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI load_movss_stream_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI load_movups_xmm_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI load_vmovups_zmm_kernel(float *, int, int64_t);
    // Store streams for the write-bandwidth rows (asm/_LDP_.S).
    void CPUFB_SYSV_ABI store_movups_xmm_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI store_vmovups_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI store_vmovups_zmm_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI store_movntps_xmm_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI store_vmovntps_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI store_vmovntps_zmm_kernel(float *, int, int64_t);
    void CPUFB_SYSV_ABI store_partial_line_kernel(float *, int, int64_t);
}
using cpufb::CacheData;

// The chase kernel and line flush of this architecture (common/cache/cache_report).
cpufb::CacheProbeKernels cache_probe_kernels();
#endif
