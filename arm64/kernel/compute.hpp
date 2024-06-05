#ifndef _COMPUTE_HPP
#define _COMPUTE_HPP

extern "C"
{
#ifdef _ASIMD_
    void asimd_fmla_vs_f32f32f32(int64_t);
    void asimd_fmla_vv_f32f32f32(int64_t);
    void asimd_fmla_vs_f64f64f64(int64_t);
    void asimd_fmla_vv_f64f64f64(int64_t);
#endif

#ifdef _ASIMD_HP_
    void asimd_fmla_vs_fp16fp16fp16(int64_t);
    void asimd_fmla_vv_fp16fp16fp16(int64_t);
#endif

#ifdef _ASIMD_DP_
    void asimd_dp4a_vs_s32s8s8(int64_t);
    void asimd_dp4a_vv_s32s8s8(int64_t);
    void asimd_dp4a_vs_u32u8u8(int64_t);
    void asimd_dp4a_vv_u32u8u8(int64_t);
#endif

#ifdef _BF16_
    void asimd_mmla_fp32bf16bf16(int64_t);
    void asimd_dp2a_vs_fp32bf16bf16(int64_t);
    void asimd_dp2a_vv_fp32bf16bf16(int64_t);
#endif

#ifdef _I8MM_
    void asimd_mmla_s32s8s8(int64_t);
    void asimd_mmla_u32u8u8(int64_t);
    void asimd_mmla_s32u8s8(int64_t);

    void asimd_dp4a_vs_s32s8u8(int64_t);
    void asimd_dp4a_vs_s32u8s8(int64_t);
    void asimd_dp4a_vv_s32u8s8(int64_t);
#endif

#ifdef _SVE_FMLA_
    void sve_fmla_vs_f32f32f32(int64_t);
#endif
}



#endif