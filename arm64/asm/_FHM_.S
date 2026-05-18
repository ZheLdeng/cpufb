.align 4
#include "common_arm64.h"

// FEAT_FHM (FP16FML) — fmlal / fmlal2
//   v.4s += v.4h * v.4h        (fmlal:  low  half lanes of the fp16 inputs)
//   v.4s += v.4h(hi) * v.4h(hi)(fmlal2: encoded with .4h operands too;
//                                       the "2" denotes upper-half source)
// Per-instruction FLOPs at 128b:
//   fmlal  v.4s, v.4h, v.4h     : 4 lanes * 2 ops = 8 FLOPS
//   fmlal2 v.4s, v.4h, v.4h     : 4 lanes * 2 ops = 8 FLOPS
//   fmlal  v.4s, v.4h, v.h[idx] : 4 lanes * 2 ops = 8 FLOPS
// 24 unrolled instructions per loop -> comp_pl = 192 FLOPs/iter.

#void asimd_fmlal_vv_f32f16f16(int64_t);
#void asimd_fmlal2_vv_f32f16f16(int64_t);
#void asimd_fmlal_vs_f32f16f16(int64_t);
#void asimd_fmlal_vv_f32f16f16_latency(int64_t);
#void asimd_fmlal2_vv_f32f16f16_latency(int64_t);
#void asimd_fmlal_pair_vv_f32f16f16(int64_t);

###############################
    PROLOGUE asimd_fmlal_vv_f32f16f16
    preserve_caller_vec
    INIT
.asimd.fmlal.vv.f32f16f16.L1:
    fmlal v8.4s,  v0.4h, v1.4h
    fmlal v9.4s,  v0.4h, v1.4h
    fmlal v10.4s, v0.4h, v1.4h
    fmlal v11.4s, v0.4h, v1.4h
    fmlal v12.4s, v0.4h, v1.4h
    fmlal v13.4s, v0.4h, v1.4h
    fmlal v14.4s, v0.4h, v1.4h
    fmlal v15.4s, v0.4h, v1.4h
    fmlal v16.4s, v0.4h, v1.4h
    fmlal v17.4s, v0.4h, v1.4h
    fmlal v18.4s, v0.4h, v1.4h
    fmlal v19.4s, v0.4h, v1.4h
    subs x0, x0, #1
    fmlal v20.4s, v0.4h, v1.4h
    fmlal v21.4s, v0.4h, v1.4h
    fmlal v22.4s, v0.4h, v1.4h
    fmlal v23.4s, v0.4h, v1.4h
    fmlal v24.4s, v0.4h, v1.4h
    fmlal v25.4s, v0.4h, v1.4h
    fmlal v26.4s, v0.4h, v1.4h
    fmlal v27.4s, v0.4h, v1.4h
    fmlal v28.4s, v0.4h, v1.4h
    fmlal v29.4s, v0.4h, v1.4h
    fmlal v30.4s, v0.4h, v1.4h
    fmlal v31.4s, v0.4h, v1.4h
    bne .asimd.fmlal.vv.f32f16f16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmlal2_vv_f32f16f16
    preserve_caller_vec
    INIT
.asimd.fmlal2.vv.f32f16f16.L1:
    fmlal2 v8.4s,  v0.4h, v1.4h
    fmlal2 v9.4s,  v0.4h, v1.4h
    fmlal2 v10.4s, v0.4h, v1.4h
    fmlal2 v11.4s, v0.4h, v1.4h
    fmlal2 v12.4s, v0.4h, v1.4h
    fmlal2 v13.4s, v0.4h, v1.4h
    fmlal2 v14.4s, v0.4h, v1.4h
    fmlal2 v15.4s, v0.4h, v1.4h
    fmlal2 v16.4s, v0.4h, v1.4h
    fmlal2 v17.4s, v0.4h, v1.4h
    fmlal2 v18.4s, v0.4h, v1.4h
    fmlal2 v19.4s, v0.4h, v1.4h
    subs x0, x0, #1
    fmlal2 v20.4s, v0.4h, v1.4h
    fmlal2 v21.4s, v0.4h, v1.4h
    fmlal2 v22.4s, v0.4h, v1.4h
    fmlal2 v23.4s, v0.4h, v1.4h
    fmlal2 v24.4s, v0.4h, v1.4h
    fmlal2 v25.4s, v0.4h, v1.4h
    fmlal2 v26.4s, v0.4h, v1.4h
    fmlal2 v27.4s, v0.4h, v1.4h
    fmlal2 v28.4s, v0.4h, v1.4h
    fmlal2 v29.4s, v0.4h, v1.4h
    fmlal2 v30.4s, v0.4h, v1.4h
    fmlal2 v31.4s, v0.4h, v1.4h
    bne .asimd.fmlal2.vv.f32f16f16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmlal_vs_f32f16f16
    preserve_caller_vec
    INIT
.asimd.fmlal.vs.f32f16f16.L1:
    fmlal v8.4s,  v0.4h, v1.h[0]
    fmlal v9.4s,  v0.4h, v1.h[0]
    fmlal v10.4s, v0.4h, v1.h[0]
    fmlal v11.4s, v0.4h, v1.h[0]
    fmlal v12.4s, v0.4h, v1.h[0]
    fmlal v13.4s, v0.4h, v1.h[0]
    fmlal v14.4s, v0.4h, v1.h[0]
    fmlal v15.4s, v0.4h, v1.h[0]
    fmlal v16.4s, v0.4h, v1.h[0]
    fmlal v17.4s, v0.4h, v1.h[0]
    fmlal v18.4s, v0.4h, v1.h[0]
    fmlal v19.4s, v0.4h, v1.h[0]
    subs x0, x0, #1
    fmlal v20.4s, v0.4h, v1.h[0]
    fmlal v21.4s, v0.4h, v1.h[0]
    fmlal v22.4s, v0.4h, v1.h[0]
    fmlal v23.4s, v0.4h, v1.h[0]
    fmlal v24.4s, v0.4h, v1.h[0]
    fmlal v25.4s, v0.4h, v1.h[0]
    fmlal v26.4s, v0.4h, v1.h[0]
    fmlal v27.4s, v0.4h, v1.h[0]
    fmlal v28.4s, v0.4h, v1.h[0]
    fmlal v29.4s, v0.4h, v1.h[0]
    fmlal v30.4s, v0.4h, v1.h[0]
    fmlal v31.4s, v0.4h, v1.h[0]
    bne .asimd.fmlal.vs.f32f16f16.L1
    restore_caller_vec
    ret
###############################
# latency: single accumulator (v8) creates a dependency chain
    PROLOGUE asimd_fmlal_vv_f32f16f16_latency
    preserve_caller_vec
    INIT
.asimd.fmlal.vv.f32f16f16.lat.L1:
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    subs x0, x0, #1
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    fmlal v8.4s, v0.4h, v1.4h
    bne .asimd.fmlal.vv.f32f16f16.lat.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmlal2_vv_f32f16f16_latency
    preserve_caller_vec
    INIT
.asimd.fmlal2.vv.f32f16f16.lat.L1:
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    subs x0, x0, #1
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    fmlal2 v8.4s, v0.4h, v1.4h
    bne .asimd.fmlal2.vv.f32f16f16.lat.L1
    restore_caller_vec
    ret
###############################
# Combined fmlal+fmlal2 alternation: full fp16 vector (8 lanes) processed
# into two fp32 .4s accumulators. Per pair: 16 FLOPs. 24 instructions
# (12 fmlal + 12 fmlal2) -> 192 FLOPs/iter. Demonstrates real-world peak.
    PROLOGUE asimd_fmlal_pair_vv_f32f16f16
    preserve_caller_vec
    INIT
.asimd.fmlal.pair.vv.f32f16f16.L1:
    fmlal  v8.4s,  v0.4h, v1.4h
    fmlal2 v9.4s,  v0.4h, v1.4h
    fmlal  v10.4s, v0.4h, v1.4h
    fmlal2 v11.4s, v0.4h, v1.4h
    fmlal  v12.4s, v0.4h, v1.4h
    fmlal2 v13.4s, v0.4h, v1.4h
    fmlal  v14.4s, v0.4h, v1.4h
    fmlal2 v15.4s, v0.4h, v1.4h
    fmlal  v16.4s, v0.4h, v1.4h
    fmlal2 v17.4s, v0.4h, v1.4h
    fmlal  v18.4s, v0.4h, v1.4h
    fmlal2 v19.4s, v0.4h, v1.4h
    subs x0, x0, #1
    fmlal  v20.4s, v0.4h, v1.4h
    fmlal2 v21.4s, v0.4h, v1.4h
    fmlal  v22.4s, v0.4h, v1.4h
    fmlal2 v23.4s, v0.4h, v1.4h
    fmlal  v24.4s, v0.4h, v1.4h
    fmlal2 v25.4s, v0.4h, v1.4h
    fmlal  v26.4s, v0.4h, v1.4h
    fmlal2 v27.4s, v0.4h, v1.4h
    fmlal  v28.4s, v0.4h, v1.4h
    fmlal2 v29.4s, v0.4h, v1.4h
    fmlal  v30.4s, v0.4h, v1.4h
    fmlal2 v31.4s, v0.4h, v1.4h
    bne .asimd.fmlal.pair.vv.f32f16f16.L1
    restore_caller_vec
    ret
