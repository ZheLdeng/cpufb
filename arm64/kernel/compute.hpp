#ifndef _COMPUTE_HPP
#define _COMPUTE_HPP

extern "C"
{
#ifdef _ASIMD_
    void asimd_fmla_vs_f32f32f32(int64_t);
    void asimd_fmla_vv_f32f32f32(int64_t);
    void asimd_fmla_vs_f64f64f64(int64_t);
    void asimd_fmla_vv_f64f64f64(int64_t);
    void asimd_fmla2_vs_f32f32f32(int64_t);
    void asimd_fmla2_vs_f64f64f64(int64_t);
    void asimd_fmla2_vv_f32f32f32(int64_t);
    void asimd_fmla2_vv_f64f64f64(int64_t);
    void asimd_hybrid_fp32_mla_6x16(int64_t);
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
    void asimd_bfmlalb_fp32bf16bf16(int64_t);
    void asimd_bfmlalt_fp32bf16bf16(int64_t);
#endif

#ifdef _FHM_
    void asimd_fmlal_vv_f32f16f16(int64_t);
    void asimd_fmlal2_vv_f32f16f16(int64_t);
    void asimd_fmlal_vs_f32f16f16(int64_t);
    void asimd_fmlal_vv_f32f16f16_latency(int64_t);
    void asimd_fmlal2_vv_f32f16f16_latency(int64_t);
    void asimd_fmlal_pair_vv_f32f16f16(int64_t);
#endif

#ifdef _I8MM_
    void asimd_mmla_s32s8s8(int64_t);
    void asimd_mmla_u32u8u8(int64_t);
    void asimd_mmla_s32u8s8(int64_t);

    void asimd_dp4a_vs_s32s8u8(int64_t);
    void asimd_dp4a_vs_s32u8s8(int64_t);
    void asimd_dp4a_vv_s32u8s8(int64_t);
#endif

#ifdef _SVE_
    void sve_fmla_vs_f32f32f32(int64_t);
    void sve_fmla_vv_f32f32f32(int64_t);
    void sve_fmla_vs_f64f64f64(int64_t);
    void sve_fmla_vv_f64f64f64(int64_t);
    void sve_fmla2_vv_f32f32f32(int64_t);
    void sve_fmla2_vv_f64f64f64(int64_t);
#endif

#ifdef _SVE_I8MM_
    void sve_mmla_s32s8s8(int64_t);
    void sve_mmla_u32u8u8(int64_t);
    void sve_mmla_s32u8s8(int64_t);
    void sve_dp4a_vv_s32s8s8(int64_t);
    void sve_dp4a_vv_s32u8u8(int64_t);
    void sve_dp4a_vv_s32u8s8(int64_t);
#endif

#ifdef _SVE_BF16_
    void sve_bfmmla_f32bf16bf16(int64_t);
    void sve_bfdot_vv_f32bf16bf16(int64_t);
    void sve_bfdot_vs_f32bf16bf16(int64_t);
#endif

#ifdef _SVE_F32MM_
    void sve_fmmla_f32f32f32(int64_t);
    void sve_fmmla_f32f32f32_latency(int64_t);
#endif

#ifdef _SVE_F64MM_
    void sve_fmmla_f64f64f64(int64_t);
    void sve_fmmla_f64f64f64_latency(int64_t);
#endif

#ifdef _SVE_FP16_FMLA_
    void sve_fmla_vv_f16f16f16(int64_t);
    void sve_fmla_vs_f16f16f16(int64_t);
    void sve_fmla_vv_f16f16f16_latency(int64_t);
#endif

#ifdef _SVE2_
    void sve2_sqrdmlah_vv_s16s16s16(int64_t);
    void sve2_sqrdmlah_vv_s16s16s16_latency(int64_t);
#endif

#ifdef _SME_
    void sme_bfmopa_vv_f32bf16bf16(int64_t);
    void sme_fmopa_vv_f32f32f32(int64_t);
    void sme_fmopa2_vv_f32f32f32(int64_t);
    void sme_fmopa_vv_f32f16f16(int64_t);
    void sme_smopa_vv_i32i8i8(int64_t);
    void sme_umopa_vv_u32u8u8(int64_t);
    void sme_usmopa_vv_s32u8s8(int64_t);
    void sme_sumopa_vv_s32s8u8(int64_t);
#endif

#ifdef _SME_F16F16_
    void sme_fmopa_vv_f16f16f16(int64_t);
    void sme_fmopa2_vv_f16f16f16(int64_t);
#endif

#ifdef _SME_I16I32_
    void sme_smopa_vv_i32i16i16(int64_t);
    void sme_umopa_vv_i32i16i16(int64_t);
    void sme_smopa2_vv_i32i16i16(int64_t);
    void sme_umopa2_vv_i32i16i16(int64_t);
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
    void sme2_fmla2_mvv_f32f32f32(int64_t);

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
    void sme_fmopa2_vv_f64f64f64(int64_t);

    void sme2_fmla_vs_f64f64f64(int64_t);
    void sme2_fmla2_vs_f64f64f64(int64_t);
    void sme2_fmla4_vs_f64f64f64(int64_t);
    void sme2_fmla_vv_f64f64f64(int64_t);
    void sme2_fmla2_vv_f64f64f64(int64_t);

    void sme2_fmla4_vv_f64f64f64(int64_t);
    void sme2_fmla_mvv_f64f64f64(int64_t);
    void sme2_fmla4_mvv_f64f64f64(int64_t);
#endif


}



#endif