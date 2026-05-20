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
    // Tier-2 additions: non-FMA paths and negated-FMA chain.
    void asimd_fmls_vv_f32f32f32(int64_t);
    void asimd_fneg_fmla_vv_f32f32f32(int64_t);
    void asimd_fadd_vv_f32f32f32(int64_t);
    void asimd_fmul_vv_f32f32f32(int64_t);
    // Tier-2 latency variants.
    void asimd_fmls_vv_f32f32f32_latency(int64_t);
    void asimd_fneg_fmla_vv_f32f32f32_latency(int64_t);
    void asimd_fadd_vv_f32f32f32_latency(int64_t);
    void asimd_fmul_vv_f32f32f32_latency(int64_t);
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
    // Latency variants.
    void asimd_dp4a_vs_s32s8s8_latency(int64_t);
    void asimd_dp4a_vv_s32s8s8_latency(int64_t);
    void asimd_dp4a_vs_u32u8u8_latency(int64_t);
    void asimd_dp4a_vv_u32u8u8_latency(int64_t);
#endif

#ifdef _BF16_
    void asimd_mmla_fp32bf16bf16(int64_t);
    void asimd_dp2a_vs_fp32bf16bf16(int64_t);
    void asimd_dp2a_vv_fp32bf16bf16(int64_t);
    void asimd_bfmlalb_fp32bf16bf16(int64_t);
    void asimd_bfmlalt_fp32bf16bf16(int64_t);
    // Latency variants.
    void asimd_mmla_fp32bf16bf16_latency(int64_t);
    void asimd_dp2a_vs_fp32bf16bf16_latency(int64_t);
    void asimd_dp2a_vv_fp32bf16bf16_latency(int64_t);
    void asimd_bfmlalb_fp32bf16bf16_latency(int64_t);
    void asimd_bfmlalt_fp32bf16bf16_latency(int64_t);
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
    // Latency variants.
    void asimd_mmla_s32s8s8_latency(int64_t);
    void asimd_mmla_u32u8u8_latency(int64_t);
    void asimd_mmla_s32u8s8_latency(int64_t);
    void asimd_dp4a_vs_s32s8u8_latency(int64_t);
    void asimd_dp4a_vs_s32u8s8_latency(int64_t);
    void asimd_dp4a_vv_s32u8s8_latency(int64_t);
#endif

#ifdef _SVE_
    void sve_fmla_vs_f32f32f32(int64_t);
    void sve_fmla_vv_f32f32f32(int64_t);
    void sve_fmla_vs_f64f64f64(int64_t);
    void sve_fmla_vv_f64f64f64(int64_t);
    void sve_fmla2_vv_f32f32f32(int64_t);
    void sve_fmla2_vv_f64f64f64(int64_t);
    // Tier-2 additions: SVE complex FCMLA + reductions.
    void sve_fcmla_vv_f32f32f32_0(int64_t);
    void sve_fcmla_vv_f32f32f32_90(int64_t);
    void sve_fcmla_vv_f32f32f32_180(int64_t);
    void sve_fcmla_vv_f32f32f32_270(int64_t);
    void sve_fcmla_vv_f64f64f64_0(int64_t);
    void sve_fcmla_vv_f64f64f64_90(int64_t);
    void sve_fcmla_vv_f64f64f64_180(int64_t);
    void sve_fcmla_vv_f64f64f64_270(int64_t);
    void sve_fadda_v_f32(int64_t);
    void sve_fadda_v_f64(int64_t);
    void sve_faddv_v_f32(int64_t);
    void sve_faddv_v_f64(int64_t);
    // Tier-2 latency variants for SVE FCMLA (sve_fadda is its own latency
    // chain by construction, registered twice in cpufb.cpp -- no new asm).
    void sve_fcmla_vv_f32f32f32_0_latency(int64_t);
    void sve_fcmla_vv_f32f32f32_90_latency(int64_t);
    void sve_fcmla_vv_f32f32f32_180_latency(int64_t);
    void sve_fcmla_vv_f32f32f32_270_latency(int64_t);
    void sve_fcmla_vv_f64f64f64_0_latency(int64_t);
    void sve_fcmla_vv_f64f64f64_90_latency(int64_t);
    void sve_fcmla_vv_f64f64f64_180_latency(int64_t);
    void sve_fcmla_vv_f64f64f64_270_latency(int64_t);
#endif

#ifdef _ASIMD_FCMA_
    void asimd_fcmla_vv_f32f32f32_0(int64_t);
    void asimd_fcmla_vv_f32f32f32_90(int64_t);
    void asimd_fcmla_vv_f32f32f32_180(int64_t);
    void asimd_fcmla_vv_f32f32f32_270(int64_t);
    void asimd_fcmla_pair_vv_f32f32f32(int64_t);
  #ifdef _ASIMD_HP_
    void asimd_fcmla_vv_f16f16f16_0(int64_t);
    void asimd_fcmla_vv_f16f16f16_90(int64_t);
    void asimd_fcmla_vv_f16f16f16_180(int64_t);
    void asimd_fcmla_vv_f16f16f16_270(int64_t);
  #endif
    // Latency variants.
    void asimd_fcmla_vv_f32f32f32_0_latency(int64_t);
    void asimd_fcmla_vv_f32f32f32_90_latency(int64_t);
    void asimd_fcmla_vv_f32f32f32_180_latency(int64_t);
    void asimd_fcmla_vv_f32f32f32_270_latency(int64_t);
  #ifdef _ASIMD_HP_
    void asimd_fcmla_vv_f16f16f16_0_latency(int64_t);
    void asimd_fcmla_vv_f16f16f16_90_latency(int64_t);
    void asimd_fcmla_vv_f16f16f16_180_latency(int64_t);
    void asimd_fcmla_vv_f16f16f16_270_latency(int64_t);
  #endif
#endif

#ifdef _ASIMD_REDUCE_
    void asimd_faddp_v_f32(int64_t);
    void asimd_fmaxv_v_f32(int64_t);
    void asimd_saddlv_v_s8(int64_t);
    void asimd_smaxv_v_s32(int64_t);
  #ifdef _ASIMD_HP_
    void asimd_fmaxv_v_f16(int64_t);
  #endif
    // Latency variant for faddp only (Pattern A'). Scalar-output reductions
    // skipped per plan because they require dup-back feedback.
    void asimd_faddp_v_f32_latency(int64_t);
#endif

#ifdef _ASIMD_RECIP_
    void asimd_frecpe_recps_v_f32(int64_t);
    void asimd_frsqrte_rsqrts_v_f32(int64_t);
  #ifdef _ASIMD_HP_
    void asimd_frecpe_recps_v_f16(int64_t);
  #endif
    // Latency variants (Pattern B: dst fed back through both ops).
    void asimd_frecpe_recps_v_f32_latency(int64_t);
    void asimd_frsqrte_rsqrts_v_f32_latency(int64_t);
  #ifdef _ASIMD_HP_
    void asimd_frecpe_recps_v_f16_latency(int64_t);
  #endif
#endif

#ifdef _ASIMD_INT_MAC_
    void asimd_mla_vs_s32s32s32(int64_t);
    void asimd_mla_vv_s32s32s32(int64_t);
    void asimd_mla_vs_s16s16s16(int64_t);
    void asimd_sqdmlal_vv_s32s16s16(int64_t);
    void asimd_sqdmlal2_vv_s32s16s16(int64_t);
    // Latency variants.
    void asimd_mla_vs_s32s32s32_latency(int64_t);
    void asimd_mla_vv_s32s32s32_latency(int64_t);
    void asimd_mla_vs_s16s16s16_latency(int64_t);
    void asimd_sqdmlal_vv_s32s16s16_latency(int64_t);
    void asimd_sqdmlal2_vv_s32s16s16_latency(int64_t);
#endif

#ifdef _ASIMD_TBL_
    void asimd_tbl_4table_v_u8(int64_t);
    void asimd_tbx_4table_v_u8(int64_t);
    // Latency variants (Pattern B: dst fed back as index).
    void asimd_tbl_4table_v_u8_latency(int64_t);
    void asimd_tbx_4table_v_u8_latency(int64_t);
#endif

#ifdef _SVE_I8MM_
    void sve_mmla_s32s8s8(int64_t);
    void sve_mmla_u32u8u8(int64_t);
    void sve_mmla_s32u8s8(int64_t);
    void sve_dp4a_vv_s32s8s8(int64_t);
    void sve_dp4a_vv_s32u8u8(int64_t);
    void sve_dp4a_vv_s32u8s8(int64_t);
    // Latency variants.
    void sve_mmla_s32s8s8_latency(int64_t);
    void sve_mmla_u32u8u8_latency(int64_t);
    void sve_mmla_s32u8s8_latency(int64_t);
    void sve_dp4a_vv_s32s8s8_latency(int64_t);
    void sve_dp4a_vv_s32u8u8_latency(int64_t);
    void sve_dp4a_vv_s32u8s8_latency(int64_t);
#endif

#ifdef _SVE_BF16_
    void sve_bfmmla_f32bf16bf16(int64_t);
    void sve_bfdot_vv_f32bf16bf16(int64_t);
    void sve_bfdot_vs_f32bf16bf16(int64_t);
    // Latency variants.
    void sve_bfmmla_f32bf16bf16_latency(int64_t);
    void sve_bfdot_vv_f32bf16bf16_latency(int64_t);
    void sve_bfdot_vs_f32bf16bf16_latency(int64_t);
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