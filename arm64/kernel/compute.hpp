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
    void sve_fmla_vv_f32f32f32(int64_t);
    void sve_fmla_vs_f64f64f64(int64_t);
    void sve_fmla_vv_f64f64f64(int64_t);
#endif

#ifdef _SME_
    void sme_bfmopa_vv_f32bf16bf16(int64_t);
    void sme_fmopa_vv_f32f32f32(int64_t);
    void sme_fmopa_vv_f32f16f16(int64_t);
    void sme_smopa_vv_i32i8i8(int64_t);
#endif
#ifdef _SME2_
    void sme2_bfmlal_vs_f32bf16bf16(int64_t);
    void sme2_bfmlal4_vs_f32bf16bf16(int64_t);
    void sme2_bfmlal_vv_f32bf16bf16(int64_t);
    void sme2_bfmlal4_vv_f32bf16bf16(int64_t);
    void sme2_bfmlal_mvv_f32bf16bf16(int64_t);
    void sme2_bfmlal4_mvv_f32bf16bf16(int64_t);

    void sme2_bfdot_vs_f32bf16bf16(int64_t);
    void sme2_bfdot4_vs_f32bf16bf16(int64_t);
    void sme2_bfdot_vv_f32bf16bf16(int64_t);
    void sme2_bfdot4_vv_f32bf16bf16(int64_t);
    void sme2_bfdot_mvv_f32bf16bf16(int64_t);
    void sme2_bfdot4_mvv_f32bf16bf16(int64_t);
    
    void sme2_fmla_vs_f32f32f32(int64_t);
    void sme2_fmla4_vs_f32f32f32(int64_t);
    void sme2_fmla_vv_f32f32f32(int64_t);
    void sme2_fmla4_vv_f32f32f32(int64_t);
    void sme2_fmla_mvv_f32f32f32(int64_t);
    void sme2_fmla4_mvv_f32f32f32(int64_t);

    void sme2_fmlal_vs_f32f16f16(int64_t);
    void sme2_fmlal4_vs_f32f16f16(int64_t);
    void sme2_fmlal_vv_f32f16f16(int64_t);
    void sme2_fmlal4_vv_f32f16f16(int64_t);
    void sme2_fmlal_mvv_f32f16f16(int64_t);
    void sme2_fmlal4_mvv_f32f16f16(int64_t);

    void sme2_fvdot_vs_f32f16f16(int64_t);
    void sme2_fvdot2_vs_f32f16f16(int64_t);

    void sme2_fdot_vs_f32f16f16(int64_t);
    void sme2_fdot4_vs_f32f16f16(int64_t);
    void sme2_fdot_vv_f32f16f16(int64_t);
    void sme2_fdot4_vv_f32f16f16(int64_t);
    void sme2_fdot_mvv_f32f16f16(int64_t);
    void sme2_fdot4_mvv_f32f16f16(int64_t);
#endif
#ifdef _SMEf64_
    void sme_fmopa_vv_f64f64f64(int64_t);
    
    void sme2_fmla_vs_f64f64f64(int64_t);
    void sme2_fmla4_vs_f64f64f64(int64_t);
    void sme2_fmla_vv_f64f64f64(int64_t);
    void sme2_fmla4_vv_f64f64f64(int64_t);
    void sme2_fmla_mvv_f64f64f64(int64_t);
    void sme2_fmla4_mvv_f64f64f64(int64_t);
#endif


}



#endif