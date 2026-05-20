.align 4
#include "common_arm64.h"

// FEAT_FCMA — NEON complex FCMLA (mandatory in ARMv8.3-a):
//   fcmla v.4s, v.4s, v.4s, #rot   ; rot ∈ {0,90,180,270}
//   fcmla v.8h, v.8h, v.8h, #rot   ; fp16 form (also needs FEAT_FP16)
// Single instruction does half a complex FMA per 2-element complex lane;
// a pair of fcmla #0 and fcmla #90 (or #180/#270) realises the full
// complex multiply-accumulate.
//
// Per-instruction op count (lane-level, conventional):
//   .4s : 4 lanes * 2 ops = 8  FLOPS / instr
//   .8h : 8 lanes * 2 ops = 16 FLOPS / instr
// 24 unrolled instructions per loop -> comp_pl = 192 (.4s), 384 (.8h).

#void asimd_fcmla_vv_f32f32f32_0(int64_t);
#void asimd_fcmla_vv_f32f32f32_90(int64_t);
#void asimd_fcmla_vv_f32f32f32_180(int64_t);
#void asimd_fcmla_vv_f32f32f32_270(int64_t);
#void asimd_fcmla_pair_vv_f32f32f32(int64_t);
#void asimd_fcmla_vv_f16f16f16_0(int64_t);
#void asimd_fcmla_vv_f16f16f16_90(int64_t);
#void asimd_fcmla_vv_f16f16f16_180(int64_t);
#void asimd_fcmla_vv_f16f16f16_270(int64_t);

.macro FCMLA_F32_LOOP rot, label
.\label:
    fcmla v8.4s , v0.4s, v1.4s, #\rot
    fcmla v9.4s , v0.4s, v1.4s, #\rot
    fcmla v10.4s, v0.4s, v1.4s, #\rot
    fcmla v11.4s, v0.4s, v1.4s, #\rot
    fcmla v12.4s, v0.4s, v1.4s, #\rot
    fcmla v13.4s, v0.4s, v1.4s, #\rot
    fcmla v14.4s, v0.4s, v1.4s, #\rot
    fcmla v15.4s, v0.4s, v1.4s, #\rot
    fcmla v16.4s, v0.4s, v1.4s, #\rot
    fcmla v17.4s, v0.4s, v1.4s, #\rot
    fcmla v18.4s, v0.4s, v1.4s, #\rot
    fcmla v19.4s, v0.4s, v1.4s, #\rot
    subs x0, x0, #1
    fcmla v20.4s, v0.4s, v1.4s, #\rot
    fcmla v21.4s, v0.4s, v1.4s, #\rot
    fcmla v22.4s, v0.4s, v1.4s, #\rot
    fcmla v23.4s, v0.4s, v1.4s, #\rot
    fcmla v24.4s, v0.4s, v1.4s, #\rot
    fcmla v25.4s, v0.4s, v1.4s, #\rot
    fcmla v26.4s, v0.4s, v1.4s, #\rot
    fcmla v27.4s, v0.4s, v1.4s, #\rot
    fcmla v28.4s, v0.4s, v1.4s, #\rot
    fcmla v29.4s, v0.4s, v1.4s, #\rot
    fcmla v30.4s, v0.4s, v1.4s, #\rot
    fcmla v31.4s, v0.4s, v1.4s, #\rot
    bne .\label
.endm

###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_0
    preserve_caller_vec
    INIT
    FCMLA_F32_LOOP 0, asimd_fcmla_vv_f32_0_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_90
    preserve_caller_vec
    INIT
    FCMLA_F32_LOOP 90, asimd_fcmla_vv_f32_90_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_180
    preserve_caller_vec
    INIT
    FCMLA_F32_LOOP 180, asimd_fcmla_vv_f32_180_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_270
    preserve_caller_vec
    INIT
    FCMLA_F32_LOOP 270, asimd_fcmla_vv_f32_270_L1
    restore_caller_vec
    ret
###############################
// pair form: fcmla #0 + fcmla #90 alternating into independent accumulators.
// 12 of each = 24 instr, equivalent to 12 full complex FMAs across 12 dst regs.
    PROLOGUE asimd_fcmla_pair_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fcmla.pair.vv.f32f32f32.L1:
    fcmla v8.4s , v0.4s, v1.4s, #0
    fcmla v8.4s , v0.4s, v1.4s, #90
    fcmla v10.4s, v0.4s, v1.4s, #0
    fcmla v10.4s, v0.4s, v1.4s, #90
    fcmla v12.4s, v0.4s, v1.4s, #0
    fcmla v12.4s, v0.4s, v1.4s, #90
    fcmla v14.4s, v0.4s, v1.4s, #0
    fcmla v14.4s, v0.4s, v1.4s, #90
    fcmla v16.4s, v0.4s, v1.4s, #0
    fcmla v16.4s, v0.4s, v1.4s, #90
    fcmla v18.4s, v0.4s, v1.4s, #0
    fcmla v18.4s, v0.4s, v1.4s, #90
    subs x0, x0, #1
    fcmla v20.4s, v0.4s, v1.4s, #0
    fcmla v20.4s, v0.4s, v1.4s, #90
    fcmla v22.4s, v0.4s, v1.4s, #0
    fcmla v22.4s, v0.4s, v1.4s, #90
    fcmla v24.4s, v0.4s, v1.4s, #0
    fcmla v24.4s, v0.4s, v1.4s, #90
    fcmla v26.4s, v0.4s, v1.4s, #0
    fcmla v26.4s, v0.4s, v1.4s, #90
    fcmla v28.4s, v0.4s, v1.4s, #0
    fcmla v28.4s, v0.4s, v1.4s, #90
    fcmla v30.4s, v0.4s, v1.4s, #0
    fcmla v30.4s, v0.4s, v1.4s, #90
    bne .asimd.fcmla.pair.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
#ifdef _ASIMD_HP_
.macro FCMLA_F16_LOOP rot, label
.\label:
    fcmla v8.8h , v0.8h, v1.8h, #\rot
    fcmla v9.8h , v0.8h, v1.8h, #\rot
    fcmla v10.8h, v0.8h, v1.8h, #\rot
    fcmla v11.8h, v0.8h, v1.8h, #\rot
    fcmla v12.8h, v0.8h, v1.8h, #\rot
    fcmla v13.8h, v0.8h, v1.8h, #\rot
    fcmla v14.8h, v0.8h, v1.8h, #\rot
    fcmla v15.8h, v0.8h, v1.8h, #\rot
    fcmla v16.8h, v0.8h, v1.8h, #\rot
    fcmla v17.8h, v0.8h, v1.8h, #\rot
    fcmla v18.8h, v0.8h, v1.8h, #\rot
    fcmla v19.8h, v0.8h, v1.8h, #\rot
    subs x0, x0, #1
    fcmla v20.8h, v0.8h, v1.8h, #\rot
    fcmla v21.8h, v0.8h, v1.8h, #\rot
    fcmla v22.8h, v0.8h, v1.8h, #\rot
    fcmla v23.8h, v0.8h, v1.8h, #\rot
    fcmla v24.8h, v0.8h, v1.8h, #\rot
    fcmla v25.8h, v0.8h, v1.8h, #\rot
    fcmla v26.8h, v0.8h, v1.8h, #\rot
    fcmla v27.8h, v0.8h, v1.8h, #\rot
    fcmla v28.8h, v0.8h, v1.8h, #\rot
    fcmla v29.8h, v0.8h, v1.8h, #\rot
    fcmla v30.8h, v0.8h, v1.8h, #\rot
    fcmla v31.8h, v0.8h, v1.8h, #\rot
    bne .\label
.endm

    PROLOGUE asimd_fcmla_vv_f16f16f16_0
    preserve_caller_vec
    INIT
    FCMLA_F16_LOOP 0, asimd_fcmla_vv_f16_0_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f16f16f16_90
    preserve_caller_vec
    INIT
    FCMLA_F16_LOOP 90, asimd_fcmla_vv_f16_90_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f16f16f16_180
    preserve_caller_vec
    INIT
    FCMLA_F16_LOOP 180, asimd_fcmla_vv_f16_180_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f16f16f16_270
    preserve_caller_vec
    INIT
    FCMLA_F16_LOOP 270, asimd_fcmla_vv_f16_270_L1
    restore_caller_vec
    ret
#endif

###############################
// Latency variants: all 24 fcmla write to v8 (Pattern A; fcmla accumulates
// into vd naturally). Use a single-dst macro to keep this section compact.

.macro FCMLA_F32_LAT_LOOP rot, label
.\label:
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    subs x0, x0, #1
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    fcmla v8.4s, v0.4s, v1.4s, #\rot
    bne .\label
.endm

###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_0_latency
    preserve_caller_vec
    INIT
    FCMLA_F32_LAT_LOOP 0, asimd_fcmla_vv_f32_0_lat_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_90_latency
    preserve_caller_vec
    INIT
    FCMLA_F32_LAT_LOOP 90, asimd_fcmla_vv_f32_90_lat_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_180_latency
    preserve_caller_vec
    INIT
    FCMLA_F32_LAT_LOOP 180, asimd_fcmla_vv_f32_180_lat_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f32f32f32_270_latency
    preserve_caller_vec
    INIT
    FCMLA_F32_LAT_LOOP 270, asimd_fcmla_vv_f32_270_lat_L1
    restore_caller_vec
    ret
###############################
#ifdef _ASIMD_HP_
.macro FCMLA_F16_LAT_LOOP rot, label
.\label:
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    subs x0, x0, #1
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    fcmla v8.8h, v0.8h, v1.8h, #\rot
    bne .\label
.endm

    PROLOGUE asimd_fcmla_vv_f16f16f16_0_latency
    preserve_caller_vec
    INIT
    FCMLA_F16_LAT_LOOP 0, asimd_fcmla_vv_f16_0_lat_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f16f16f16_90_latency
    preserve_caller_vec
    INIT
    FCMLA_F16_LAT_LOOP 90, asimd_fcmla_vv_f16_90_lat_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f16f16f16_180_latency
    preserve_caller_vec
    INIT
    FCMLA_F16_LAT_LOOP 180, asimd_fcmla_vv_f16_180_lat_L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fcmla_vv_f16f16f16_270_latency
    preserve_caller_vec
    INIT
    FCMLA_F16_LAT_LOOP 270, asimd_fcmla_vv_f16_270_lat_L1
    restore_caller_vec
    ret
#endif
