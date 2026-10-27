#ifndef CPUFB_X64_COMPUTE_HPP
#define CPUFB_X64_COMPUTE_HPP

#include "sys/calling_convention.hpp"

extern "C"
{
#ifdef _AVX2_
    void CPUFB_SYSV_ABI avx2_add_s32(int64_t, void *);
    void CPUFB_SYSV_ABI avx2_add_s32_latency(int64_t, void *);
    void CPUFB_SYSV_ABI avx2_mul_s32(int64_t, void *);
    void CPUFB_SYSV_ABI avx2_mul_s32_latency(int64_t, void *);
#endif

#ifdef _AVX512_IFMA_
    void CPUFB_SYSV_ABI avx512_ifma_madd52_u64(int64_t, void *);
    void CPUFB_SYSV_ABI avx512_ifma_madd52_u64_latency(int64_t, void *);
#endif

#ifdef _AVX512_VBMI_
    void CPUFB_SYSV_ABI avx512_vbmi_permb_u8(int64_t, void *);
    void CPUFB_SYSV_ABI avx512_vbmi_permb_u8_latency(int64_t, void *);
#endif

#ifdef _AVX512_VPOPCNTDQ_
    void CPUFB_SYSV_ABI avx512_vpopcnt_s32(int64_t, void *);
    void CPUFB_SYSV_ABI avx512_vpopcnt_s32_latency(int64_t, void *);
#endif

#ifdef _AMX_BF16_
    void CPUFB_SYSV_ABI amx_bf16_mm_f32bf16bf16(int64_t, void *tile_cfg);
    void CPUFB_SYSV_ABI amx_bf16_mm_f32bf16bf16_latency(
        int64_t, void *tile_cfg);

#endif

#ifdef _AMX_INT8_
    void CPUFB_SYSV_ABI amx_int8_mm_s32s8s8(int64_t, void *tile_cfg);
    void CPUFB_SYSV_ABI amx_int8_mm_s32s8u8(int64_t, void *tile_cfg);
    void CPUFB_SYSV_ABI amx_int8_mm_s32u8s8(int64_t, void *tile_cfg);
    void CPUFB_SYSV_ABI amx_int8_mm_s32u8u8(int64_t, void *tile_cfg);
    void CPUFB_SYSV_ABI amx_int8_mm_s32s8s8_latency(int64_t, void *tile_cfg);
#endif

#ifdef _AVX_
    void CPUFB_SYSV_ABI avx_add_mul_f32f32_f32(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_add_mul_f64f64_f64(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_add_mul_f32f32_f32_latency(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_add_mul_f64f64_f64_latency(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_add_f32(int64_t, void *);
    void CPUFB_SYSV_ABI avx_add_f32_latency(int64_t, void *);
    void CPUFB_SYSV_ABI avx_mul_f32(int64_t, void *);
    void CPUFB_SYSV_ABI avx_mul_f32_latency(int64_t, void *);
    void CPUFB_SYSV_ABI avx_add_f64(int64_t, void *);
    void CPUFB_SYSV_ABI avx_add_f64_latency(int64_t, void *);
    void CPUFB_SYSV_ABI avx_mul_f64(int64_t, void *);
    void CPUFB_SYSV_ABI avx_mul_f64_latency(int64_t, void *);
#endif

#ifdef _AVX_VNNI_
    void CPUFB_SYSV_ABI avx_vnni_dp4a_s32u8s8(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_vnni_dp2a_s32s16s16(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_vnni_dp4a_s32u8s8_latency(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_vnni_dp2a_s32s16s16_latency(int64_t, void *params);
#endif

#ifdef _AVX_VNNI_INT8_
    void CPUFB_SYSV_ABI avx_vnni_int8_dp4a_s32s8s8(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_vnni_int8_dp4a_s32s8u8(int64_t, void *params);
    void CPUFB_SYSV_ABI avx_vnni_int8_dp4a_s32u8u8(int64_t, void *params);
#endif

#ifdef _AVX512_BF16_
    void CPUFB_SYSV_ABI avx512_bf16_dp2a_f32bf16bf16(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512_bf16_dp2a_f32bf16bf16_latency(
        int64_t, void *params);
#endif

#ifdef _AVX512_FP16_
    void CPUFB_SYSV_ABI avx512_fp16_fma_f16f16f16(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512_fp16_fma_f16f16f16_latency(
        int64_t, void *params);
#endif

#ifdef _AVX512_VNNI_
    void CPUFB_SYSV_ABI avx512_vnni_dp4a_s32u8s8(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512_vnni_dp2a_s32s16s16(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512_vnni_dp4a_s32u8s8_latency(
        int64_t, void *params);
    void CPUFB_SYSV_ABI avx512_vnni_dp2a_s32s16s16_latency(
        int64_t, void *params);
#endif

#ifdef _AVX512F_
    void CPUFB_SYSV_ABI avx512f_fma_f32f32f32(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512f_fma_f64f64f64(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512f_fma_f32f32f32_latency(int64_t, void *params);
    void CPUFB_SYSV_ABI avx512f_fma_f64f64f64_latency(int64_t, void *params);
#endif

#ifdef _FMA_
    void CPUFB_SYSV_ABI fma_f32f32f32(int64_t, void *params);
    void CPUFB_SYSV_ABI fma_f64f64f64(int64_t, void *params);
    void CPUFB_SYSV_ABI fma_f32f32f32_latency(int64_t, void *params);
    void CPUFB_SYSV_ABI fma_f64f64f64_latency(int64_t, void *params);
#endif

#ifdef _SSE_
    void CPUFB_SYSV_ABI sse_add_mul_f32f32_f32(int64_t, void *params);
    void CPUFB_SYSV_ABI sse_add_f32(int64_t, void *);
    void CPUFB_SYSV_ABI sse_add_f32_latency(int64_t, void *);
    void CPUFB_SYSV_ABI sse_mul_f32(int64_t, void *);
    void CPUFB_SYSV_ABI sse_mul_f32_latency(int64_t, void *);
#endif

#ifdef _SSE2_
    void CPUFB_SYSV_ABI sse2_add_mul_f64f64_f64(int64_t, void *params);
    void CPUFB_SYSV_ABI sse2_add_f64(int64_t, void *);
    void CPUFB_SYSV_ABI sse2_add_f64_latency(int64_t, void *);
    void CPUFB_SYSV_ABI sse2_mul_f64(int64_t, void *);
    void CPUFB_SYSV_ABI sse2_mul_f64_latency(int64_t, void *);
#endif
}

#endif
