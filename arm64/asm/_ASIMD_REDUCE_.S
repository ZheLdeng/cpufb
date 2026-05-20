.align 4
#include "common_arm64.h"

// Cross-lane reductions / pairwise primitives:
//   faddp   v.4s, v.4s, v.4s  ; pairwise add (no full-vector FADDV exists for
//                               NEON FP, so the throughput proxy for fp add
//                               reduction is faddp issued back-to-back)
//   fmaxv   s_d , v.4s        ; full-vector max -> scalar (f32)
//   fmaxv   h_d , v.8h        ; full-vector max -> scalar (f16, FP16 only)
//   saddlv  h_d , v.16b       ; signed widening sum across all lanes
//   smaxv   s_d , v.4s        ; integer max reduction
// Destination of *v forms is a SIMD scalar reg, written zero-extended into Vd.
// Using 24 distinct destination regs (v8-v31) gives 24 independent issue slots.

#void asimd_faddp_v_f32(int64_t);
#void asimd_fmaxv_v_f32(int64_t);
#void asimd_saddlv_v_s8(int64_t);
#void asimd_smaxv_v_s32(int64_t);
#void asimd_fmaxv_v_f16(int64_t);

###############################
// faddp v.4s, v.4s, v.4s : pairs adjacent lanes -> 4 fp adds / instr.
    PROLOGUE asimd_faddp_v_f32
    preserve_caller_vec
    INIT
.asimd.faddp.v.f32.L1:
    faddp v8.4s , v0.4s, v1.4s
    faddp v9.4s , v0.4s, v1.4s
    faddp v10.4s, v0.4s, v1.4s
    faddp v11.4s, v0.4s, v1.4s
    faddp v12.4s, v0.4s, v1.4s
    faddp v13.4s, v0.4s, v1.4s
    faddp v14.4s, v0.4s, v1.4s
    faddp v15.4s, v0.4s, v1.4s
    faddp v16.4s, v0.4s, v1.4s
    faddp v17.4s, v0.4s, v1.4s
    faddp v18.4s, v0.4s, v1.4s
    faddp v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    faddp v20.4s, v0.4s, v1.4s
    faddp v21.4s, v0.4s, v1.4s
    faddp v22.4s, v0.4s, v1.4s
    faddp v23.4s, v0.4s, v1.4s
    faddp v24.4s, v0.4s, v1.4s
    faddp v25.4s, v0.4s, v1.4s
    faddp v26.4s, v0.4s, v1.4s
    faddp v27.4s, v0.4s, v1.4s
    faddp v28.4s, v0.4s, v1.4s
    faddp v29.4s, v0.4s, v1.4s
    faddp v30.4s, v0.4s, v1.4s
    faddp v31.4s, v0.4s, v1.4s
    bne .asimd.faddp.v.f32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmaxv_v_f32
    preserve_caller_vec
    INIT
.asimd.fmaxv.v.f32.L1:
    fmaxv s8 , v0.4s
    fmaxv s9 , v0.4s
    fmaxv s10, v0.4s
    fmaxv s11, v0.4s
    fmaxv s12, v0.4s
    fmaxv s13, v0.4s
    fmaxv s14, v0.4s
    fmaxv s15, v0.4s
    fmaxv s16, v0.4s
    fmaxv s17, v0.4s
    fmaxv s18, v0.4s
    fmaxv s19, v0.4s
    subs x0, x0, #1
    fmaxv s20, v0.4s
    fmaxv s21, v0.4s
    fmaxv s22, v0.4s
    fmaxv s23, v0.4s
    fmaxv s24, v0.4s
    fmaxv s25, v0.4s
    fmaxv s26, v0.4s
    fmaxv s27, v0.4s
    fmaxv s28, v0.4s
    fmaxv s29, v0.4s
    fmaxv s30, v0.4s
    fmaxv s31, v0.4s
    bne .asimd.fmaxv.v.f32.L1
    restore_caller_vec
    ret
###############################
// saddlv h_d, v.16b : sign-extend each .b lane and sum -> 16-bit scalar
    PROLOGUE asimd_saddlv_v_s8
    preserve_caller_vec
    INIT
.asimd.saddlv.v.s8.L1:
    saddlv h8 , v0.16b
    saddlv h9 , v0.16b
    saddlv h10, v0.16b
    saddlv h11, v0.16b
    saddlv h12, v0.16b
    saddlv h13, v0.16b
    saddlv h14, v0.16b
    saddlv h15, v0.16b
    saddlv h16, v0.16b
    saddlv h17, v0.16b
    saddlv h18, v0.16b
    saddlv h19, v0.16b
    subs x0, x0, #1
    saddlv h20, v0.16b
    saddlv h21, v0.16b
    saddlv h22, v0.16b
    saddlv h23, v0.16b
    saddlv h24, v0.16b
    saddlv h25, v0.16b
    saddlv h26, v0.16b
    saddlv h27, v0.16b
    saddlv h28, v0.16b
    saddlv h29, v0.16b
    saddlv h30, v0.16b
    saddlv h31, v0.16b
    bne .asimd.saddlv.v.s8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_smaxv_v_s32
    preserve_caller_vec
    INIT
.asimd.smaxv.v.s32.L1:
    smaxv s8 , v0.4s
    smaxv s9 , v0.4s
    smaxv s10, v0.4s
    smaxv s11, v0.4s
    smaxv s12, v0.4s
    smaxv s13, v0.4s
    smaxv s14, v0.4s
    smaxv s15, v0.4s
    smaxv s16, v0.4s
    smaxv s17, v0.4s
    smaxv s18, v0.4s
    smaxv s19, v0.4s
    subs x0, x0, #1
    smaxv s20, v0.4s
    smaxv s21, v0.4s
    smaxv s22, v0.4s
    smaxv s23, v0.4s
    smaxv s24, v0.4s
    smaxv s25, v0.4s
    smaxv s26, v0.4s
    smaxv s27, v0.4s
    smaxv s28, v0.4s
    smaxv s29, v0.4s
    smaxv s30, v0.4s
    smaxv s31, v0.4s
    bne .asimd.smaxv.v.s32.L1
    restore_caller_vec
    ret
###############################
#ifdef _ASIMD_HP_
    PROLOGUE asimd_fmaxv_v_f16
    preserve_caller_vec
    INIT
.asimd.fmaxv.v.f16.L1:
    fmaxv h8 , v0.8h
    fmaxv h9 , v0.8h
    fmaxv h10, v0.8h
    fmaxv h11, v0.8h
    fmaxv h12, v0.8h
    fmaxv h13, v0.8h
    fmaxv h14, v0.8h
    fmaxv h15, v0.8h
    fmaxv h16, v0.8h
    fmaxv h17, v0.8h
    fmaxv h18, v0.8h
    fmaxv h19, v0.8h
    subs x0, x0, #1
    fmaxv h20, v0.8h
    fmaxv h21, v0.8h
    fmaxv h22, v0.8h
    fmaxv h23, v0.8h
    fmaxv h24, v0.8h
    fmaxv h25, v0.8h
    fmaxv h26, v0.8h
    fmaxv h27, v0.8h
    fmaxv h28, v0.8h
    fmaxv h29, v0.8h
    fmaxv h30, v0.8h
    fmaxv h31, v0.8h
    bne .asimd.fmaxv.v.f16.L1
    restore_caller_vec
    ret
#endif

###############################
// Latency variant for faddp only (Pattern A': vd is not RMW; feed v8 back
// into vn). The other reductions in this file write a *scalar* output, so
// chaining them requires dup/mov feedback that conflates dup latency into
// the reading -- those are intentionally skipped (see plan).
#void asimd_faddp_v_f32_latency(int64_t);
    PROLOGUE asimd_faddp_v_f32_latency
    preserve_caller_vec
    INIT
.asimd.faddp.v.f32.lat.L1:
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    subs x0, x0, #1
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    faddp v8.4s, v8.4s, v0.4s
    bne .asimd.faddp.v.f32.lat.L1
    restore_caller_vec
    ret
