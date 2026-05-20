.align 4
#include "common_arm64.h"

// NEON integer multiply-accumulate variants from base ARMv8.0:
//   * mla   v.4s, v.4s, v.s[i]   ; integer indexed MAC, fp-style broadcast
//   * mla   v.4s, v.4s, v.4s     ; integer vector-vector MAC
//   * mla   v.8h, v.8h, v.h[i]   ; 16-bit indexed MAC (DSP)
//   * sqdmlal  v.4s, v.4h, v.4h  ; saturating doubling MAC: s32 += sat(2 * s16 * s16)
//   * sqdmlal2 v.4s, v.8h, v.8h  ; same but uses upper-half of input vectors
// Per .4s instruction: 4 lane * 2 op (mul+add)            = 8  ops/instr
// Per .8h indexed:    8 lane * 2 op                       = 16 ops/instr
// 24 unrolled instructions per loop -> comp_pl as listed.

#void asimd_mla_vs_s32s32s32(int64_t);
#void asimd_mla_vv_s32s32s32(int64_t);
#void asimd_mla_vs_s16s16s16(int64_t);
#void asimd_sqdmlal_vv_s32s16s16(int64_t);
#void asimd_sqdmlal2_vv_s32s16s16(int64_t);

###############################
    PROLOGUE asimd_mla_vs_s32s32s32
    preserve_caller_vec
    INIT
.asimd.mla.vs.s32s32s32.L1:
    mla v8.4s , v0.4s, v1.s[0]
    mla v9.4s , v0.4s, v1.s[0]
    mla v10.4s, v0.4s, v1.s[0]
    mla v11.4s, v0.4s, v1.s[0]
    mla v12.4s, v0.4s, v1.s[0]
    mla v13.4s, v0.4s, v1.s[0]
    mla v14.4s, v0.4s, v1.s[0]
    mla v15.4s, v0.4s, v1.s[0]
    mla v16.4s, v0.4s, v1.s[0]
    mla v17.4s, v0.4s, v1.s[0]
    mla v18.4s, v0.4s, v1.s[0]
    mla v19.4s, v0.4s, v1.s[0]
    subs x0, x0, #1
    mla v20.4s, v0.4s, v1.s[0]
    mla v21.4s, v0.4s, v1.s[0]
    mla v22.4s, v0.4s, v1.s[0]
    mla v23.4s, v0.4s, v1.s[0]
    mla v24.4s, v0.4s, v1.s[0]
    mla v25.4s, v0.4s, v1.s[0]
    mla v26.4s, v0.4s, v1.s[0]
    mla v27.4s, v0.4s, v1.s[0]
    mla v28.4s, v0.4s, v1.s[0]
    mla v29.4s, v0.4s, v1.s[0]
    mla v30.4s, v0.4s, v1.s[0]
    mla v31.4s, v0.4s, v1.s[0]
    bne .asimd.mla.vs.s32s32s32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_mla_vv_s32s32s32
    preserve_caller_vec
    INIT
.asimd.mla.vv.s32s32s32.L1:
    mla v8.4s , v0.4s, v1.4s
    mla v9.4s , v0.4s, v1.4s
    mla v10.4s, v0.4s, v1.4s
    mla v11.4s, v0.4s, v1.4s
    mla v12.4s, v0.4s, v1.4s
    mla v13.4s, v0.4s, v1.4s
    mla v14.4s, v0.4s, v1.4s
    mla v15.4s, v0.4s, v1.4s
    mla v16.4s, v0.4s, v1.4s
    mla v17.4s, v0.4s, v1.4s
    mla v18.4s, v0.4s, v1.4s
    mla v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    mla v20.4s, v0.4s, v1.4s
    mla v21.4s, v0.4s, v1.4s
    mla v22.4s, v0.4s, v1.4s
    mla v23.4s, v0.4s, v1.4s
    mla v24.4s, v0.4s, v1.4s
    mla v25.4s, v0.4s, v1.4s
    mla v26.4s, v0.4s, v1.4s
    mla v27.4s, v0.4s, v1.4s
    mla v28.4s, v0.4s, v1.4s
    mla v29.4s, v0.4s, v1.4s
    mla v30.4s, v0.4s, v1.4s
    mla v31.4s, v0.4s, v1.4s
    bne .asimd.mla.vv.s32s32s32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_mla_vs_s16s16s16
    preserve_caller_vec
    INIT
.asimd.mla.vs.s16s16s16.L1:
    mla v8.8h , v0.8h, v1.h[0]
    mla v9.8h , v0.8h, v1.h[0]
    mla v10.8h, v0.8h, v1.h[0]
    mla v11.8h, v0.8h, v1.h[0]
    mla v12.8h, v0.8h, v1.h[0]
    mla v13.8h, v0.8h, v1.h[0]
    mla v14.8h, v0.8h, v1.h[0]
    mla v15.8h, v0.8h, v1.h[0]
    mla v16.8h, v0.8h, v1.h[0]
    mla v17.8h, v0.8h, v1.h[0]
    mla v18.8h, v0.8h, v1.h[0]
    mla v19.8h, v0.8h, v1.h[0]
    subs x0, x0, #1
    mla v20.8h, v0.8h, v1.h[0]
    mla v21.8h, v0.8h, v1.h[0]
    mla v22.8h, v0.8h, v1.h[0]
    mla v23.8h, v0.8h, v1.h[0]
    mla v24.8h, v0.8h, v1.h[0]
    mla v25.8h, v0.8h, v1.h[0]
    mla v26.8h, v0.8h, v1.h[0]
    mla v27.8h, v0.8h, v1.h[0]
    mla v28.8h, v0.8h, v1.h[0]
    mla v29.8h, v0.8h, v1.h[0]
    mla v30.8h, v0.8h, v1.h[0]
    mla v31.8h, v0.8h, v1.h[0]
    bne .asimd.mla.vs.s16s16s16.L1
    restore_caller_vec
    ret
###############################
// sqdmlal v.4s, v.4h, v.4h : saturating doubling multiply-accumulate
// Each .4s instr: 4 lanes, op count is mul + double + add = 3 ops/lane,
// but conventionally MAC counts as 2 ops/lane (mul + add) -> 8 ops/instr.
    PROLOGUE asimd_sqdmlal_vv_s32s16s16
    preserve_caller_vec
    INIT
.asimd.sqdmlal.vv.s32s16s16.L1:
    sqdmlal v8.4s , v0.4h, v1.4h
    sqdmlal v9.4s , v0.4h, v1.4h
    sqdmlal v10.4s, v0.4h, v1.4h
    sqdmlal v11.4s, v0.4h, v1.4h
    sqdmlal v12.4s, v0.4h, v1.4h
    sqdmlal v13.4s, v0.4h, v1.4h
    sqdmlal v14.4s, v0.4h, v1.4h
    sqdmlal v15.4s, v0.4h, v1.4h
    sqdmlal v16.4s, v0.4h, v1.4h
    sqdmlal v17.4s, v0.4h, v1.4h
    sqdmlal v18.4s, v0.4h, v1.4h
    sqdmlal v19.4s, v0.4h, v1.4h
    subs x0, x0, #1
    sqdmlal v20.4s, v0.4h, v1.4h
    sqdmlal v21.4s, v0.4h, v1.4h
    sqdmlal v22.4s, v0.4h, v1.4h
    sqdmlal v23.4s, v0.4h, v1.4h
    sqdmlal v24.4s, v0.4h, v1.4h
    sqdmlal v25.4s, v0.4h, v1.4h
    sqdmlal v26.4s, v0.4h, v1.4h
    sqdmlal v27.4s, v0.4h, v1.4h
    sqdmlal v28.4s, v0.4h, v1.4h
    sqdmlal v29.4s, v0.4h, v1.4h
    sqdmlal v30.4s, v0.4h, v1.4h
    sqdmlal v31.4s, v0.4h, v1.4h
    bne .asimd.sqdmlal.vv.s32s16s16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_sqdmlal2_vv_s32s16s16
    preserve_caller_vec
    INIT
.asimd.sqdmlal2.vv.s32s16s16.L1:
    sqdmlal2 v8.4s , v0.8h, v1.8h
    sqdmlal2 v9.4s , v0.8h, v1.8h
    sqdmlal2 v10.4s, v0.8h, v1.8h
    sqdmlal2 v11.4s, v0.8h, v1.8h
    sqdmlal2 v12.4s, v0.8h, v1.8h
    sqdmlal2 v13.4s, v0.8h, v1.8h
    sqdmlal2 v14.4s, v0.8h, v1.8h
    sqdmlal2 v15.4s, v0.8h, v1.8h
    sqdmlal2 v16.4s, v0.8h, v1.8h
    sqdmlal2 v17.4s, v0.8h, v1.8h
    sqdmlal2 v18.4s, v0.8h, v1.8h
    sqdmlal2 v19.4s, v0.8h, v1.8h
    subs x0, x0, #1
    sqdmlal2 v20.4s, v0.8h, v1.8h
    sqdmlal2 v21.4s, v0.8h, v1.8h
    sqdmlal2 v22.4s, v0.8h, v1.8h
    sqdmlal2 v23.4s, v0.8h, v1.8h
    sqdmlal2 v24.4s, v0.8h, v1.8h
    sqdmlal2 v25.4s, v0.8h, v1.8h
    sqdmlal2 v26.4s, v0.8h, v1.8h
    sqdmlal2 v27.4s, v0.8h, v1.8h
    sqdmlal2 v28.4s, v0.8h, v1.8h
    sqdmlal2 v29.4s, v0.8h, v1.8h
    sqdmlal2 v30.4s, v0.8h, v1.8h
    sqdmlal2 v31.4s, v0.8h, v1.8h
    bne .asimd.sqdmlal2.vv.s32s16s16.L1
    restore_caller_vec
    ret

###############################
// Latency variants: all 24 instructions write to v8 (Pattern A; mla/sqdmlal
// accumulate into vd naturally).
###############################
#void asimd_mla_vs_s32s32s32_latency(int64_t);
    PROLOGUE asimd_mla_vs_s32s32s32_latency
    preserve_caller_vec
    INIT
.asimd.mla.vs.s32s32s32.lat.L1:
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    subs x0, x0, #1
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    mla v8.4s, v0.4s, v1.s[0]
    bne .asimd.mla.vs.s32s32s32.lat.L1
    restore_caller_vec
    ret
###############################
#void asimd_mla_vv_s32s32s32_latency(int64_t);
    PROLOGUE asimd_mla_vv_s32s32s32_latency
    preserve_caller_vec
    INIT
.asimd.mla.vv.s32s32s32.lat.L1:
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    subs x0, x0, #1
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    mla v8.4s, v0.4s, v1.4s
    bne .asimd.mla.vv.s32s32s32.lat.L1
    restore_caller_vec
    ret
###############################
#void asimd_mla_vs_s16s16s16_latency(int64_t);
    PROLOGUE asimd_mla_vs_s16s16s16_latency
    preserve_caller_vec
    INIT
.asimd.mla.vs.s16s16s16.lat.L1:
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    subs x0, x0, #1
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    mla v8.8h, v0.8h, v1.h[0]
    bne .asimd.mla.vs.s16s16s16.lat.L1
    restore_caller_vec
    ret
###############################
#void asimd_sqdmlal_vv_s32s16s16_latency(int64_t);
    PROLOGUE asimd_sqdmlal_vv_s32s16s16_latency
    preserve_caller_vec
    INIT
.asimd.sqdmlal.vv.s32s16s16.lat.L1:
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    subs x0, x0, #1
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    sqdmlal v8.4s, v0.4h, v1.4h
    bne .asimd.sqdmlal.vv.s32s16s16.lat.L1
    restore_caller_vec
    ret
###############################
#void asimd_sqdmlal2_vv_s32s16s16_latency(int64_t);
    PROLOGUE asimd_sqdmlal2_vv_s32s16s16_latency
    preserve_caller_vec
    INIT
.asimd.sqdmlal2.vv.s32s16s16.lat.L1:
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    subs x0, x0, #1
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    sqdmlal2 v8.4s, v0.8h, v1.8h
    bne .asimd.sqdmlal2.vv.s32s16s16.lat.L1
    restore_caller_vec
    ret
