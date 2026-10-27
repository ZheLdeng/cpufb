#ifndef CPUFB_ARM64_LOAD_HPP
#define CPUFB_ARM64_LOAD_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "cache/cache_report.hpp"

struct tpool;
typedef struct tpool tpool_t;
extern std::vector<double> freq;
extern "C"
{
    void load_ldp_kernel(float *, int, int64_t);
    void load_ldr_kernel(float *, int64_t);
    void load_neon_ld1b_kernel(float *, int, int64_t);
    void load_neon_ld1h_kernel(float *, int, int64_t);
    void load_neon_ld1h_4x1_kernel(float *, int, int64_t);
    void load_ldrq_4x1_offset_kernel(float *, int, int64_t);
    void load_neon_ld1w_kernel(float *, int, int64_t);
    void load_neon_ld1d_kernel(float *, int, int64_t);
    // Store streams for the write-bandwidth rows (asm/_LDP_.S).
    void store_neon_st1h_4x1_kernel(float *, int, int64_t);
    void store_neon_stnp_kernel(float *, int, int64_t);
    void store_neon_partial_line_kernel(float *, int, int64_t);
#ifdef _SVE_
    uint64_t load_sve_vector_bytes(void);
    void load_sve_ld1b_kernel(float *, int, int64_t);
    void load_sve_ld1h_kernel(float *, int, int64_t);
    void load_ld1w_kernel(float *, int, int64_t);
    void load_sve_ld1d_kernel(float *, int, int64_t);
    void store_sve_st1h_kernel(float *, int, int64_t);
    void store_sve_stnt1h_kernel(float *, int, int64_t);
#endif
#ifdef _SME_
    uint64_t load_sme_vector_bytes(void);
    void sme_ldr_kernel(float *, int, int);
    void sme_ldr2_kernel(float *, int, int);
    void sme_ld1wV_kernel(float *, int, int);
    void sme_ld1wH_kernel(float *, int, int);
#endif
#ifdef _SME2_
    void sme_ld1w_kernel(float *, int, int);
#endif
}
using cpufb::CacheData;

// The chase kernel and line flush of this architecture (common/cache/cache_report).
cpufb::CacheProbeKernels cache_probe_kernels();
#endif
