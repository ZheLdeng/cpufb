#ifndef _COMPUTE_HPP
#define _COMPUTE_HPP

extern "C"
{
#ifdef _AMX_BF16_
    void amx_bf16_mm_f32bf16bf16(int64_t, void* tile_cfg);

#endif

#ifdef _AMX_INT8_
    void amx_int8_mm_s32s8s8(int64_t, void* tile_cfg);
    void amx_int8_mm_s32s8u8(int64_t, void* tile_cfg);
    void amx_int8_mm_s32u8s8(int64_t, void* tile_cfg);
    void amx_int8_mm_s32u8u8(int64_t, void* tile_cfg);
#endif

#ifdef _AVX_
    void avx_add_mul_f32f32_f32(int64_t, void *params);
    void avx_add_mul_f64f64_f64(int64_t, void *params);
#endif

#ifdef _AVX_VNNI_
    void avx_vnni_dp4a_s32u8s8(int64_t, void *params);
    void avx_vnni_dp2a_s32s16s16(int64_t, void *params);
#endif

#ifdef _AVX_VNNI_INT8_
    void avx_vnni_int8_dp4a_s32s8s8(int64_t, void *params);
    void avx_vnni_int8_dp4a_s32s8u8(int64_t, void *params);
    void avx_vnni_int8_dp4a_s32u8u8(int64_t, void *params);
#endif

#ifdef _AVX512_BF16_
    void avx512_bf16_dp2a_f32bf16bf16(int64_t, void *params);
#endif

#ifdef _AVX512_FP16_
    void avx512_fp16_fma_f16f16f16(int64_t, void *params);
#endif

#ifdef _AVX512_VNNI_
    void avx512_vnni_dp4a_s32u8s8(int64_t, void *params);
    void avx512_vnni_dp2a_s32s16s16(int64_t, void *params);
#endif

#ifdef _AVX512F_
    void avx512f_fma_f32f32f32(int64_t, void *params);
    void avx512f_fma_f64f64f64(int64_t, void *params);
#endif

#ifdef _FMA_
    void fma_f32f32f32(int64_t, void *params);
    void fma_f64f64f64(int64_t, void *params);
#endif

#ifdef _SSE_
    void sse_add_mul_f32f32_f32(int64_t, void *params);
#endif

#ifdef _SSE2_
    void sse2_add_mul_f64f64_f64(int64_t, void *params);
#endif

}

// extern "C"
// {
// #ifdef _SSE_
//     void sse_add_mul_f32f32_f32(int64_t, void *params);
// #endif

// #ifdef _SSE2_
//     void sse2_add_mul_f64f64_f64(int64_t, void *params);
// #endif

// #ifdef _AVX_
//     void avx_add_mul_f32f32_f32(int64_t, void *params);
//     void avx_add_mul_f64f64_f64(int64_t, void *params);
// #endif

// #ifdef _FMA_
//     void fma_f32f32f32(int64_t, void *params);
//     void fma_f64f64f64(int64_t, void *params);
// #endif

// #ifdef _AVX512F_
//     void avx512f_fma_f32f32f32(int64_t, void *params);
//     void avx512f_fma_f64f64f64(int64_t, void *params);
// #endif

// #ifdef _AVX512_BF16_
//     void avx512_bf16_dp2a_f32bf16bf16(int64_t, void *params);
// #endif

// #ifdef _AVX512_FP16_
//     void avx512_fp16_fma_f16f16f16(int64_t, void *params);
// #endif

// #ifdef _AVX512_VNNI_
//     void avx512_vnni_dp4a_s32u8s8(int64_t, void *params);
//     void avx512_vnni_dp2a_s32s16s16(int64_t, void *params);
// #endif

// #ifdef _AVX_VNNI_
//     void avx_vnni_dp4a_s32u8s8(int64_t, void *params);
//     void avx_vnni_dp2a_s32s16s16(int64_t, void *params);
// #endif

// #ifdef _AVX_VNNI_INT8_
//     void avx_vnni_int8_dp4a_s32s8s8(int64_t, void *params);
//     void avx_vnni_int8_dp4a_s32s8u8(int64_t, void *params);
//     void avx_vnni_int8_dp4a_s32u8u8(int64_t, void *params);
// #endif

// #ifdef _AMX_INT8_
//     void amx_int8_mm_s32s8s8(int64_t, void* tile_cfg);
//     void amx_int8_mm_s32s8u8(int64_t, void* tile_cfg);
//     void amx_int8_mm_s32u8s8(int64_t, void* tile_cfg);
//     void amx_int8_mm_s32u8u8(int64_t, void* tile_cfg);
// #endif
// #ifdef _AMX_BF16_
//     void amx_bf16_mm_f32bf16bf16(int64_t, void* tile_cfg);
// #endif
// }


#endif