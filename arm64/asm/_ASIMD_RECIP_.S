.align 4
#include "common_arm64.h"

// Reciprocal estimate + Newton-Raphson refinement step pairs.
//   frecpe  v.4s, v.4s             ; reciprocal estimate ~ 1/x  (1 op/lane)
//   frecps  v.4s, v.4s, v.4s       ; refinement step (mul-sub)  (~2 op/lane)
//   frsqrte v.4s, v.4s             ; reciprocal sqrt estimate   (1 op/lane)
//   frsqrts v.4s, v.4s, v.4s       ; rsqrt refinement step      (~2 op/lane)
// Pair body: 12 estimates + 12 refinement steps interleaved -> simulates one
// full Newton iteration per lane. comp_pl reported per the per-lane op count
// in the registration block; here we use 24 instructions / loop in the asm.

#void asimd_frecpe_recps_v_f32(int64_t);
#void asimd_frsqrte_rsqrts_v_f32(int64_t);
#void asimd_frecpe_recps_v_f16(int64_t);

###############################
    PROLOGUE asimd_frecpe_recps_v_f32
    preserve_caller_vec
    INIT
.asimd.frecpe.recps.v.f32.L1:
    frecpe  v8.4s , v0.4s
    frecps  v9.4s , v0.4s, v1.4s
    frecpe  v10.4s, v0.4s
    frecps  v11.4s, v0.4s, v1.4s
    frecpe  v12.4s, v0.4s
    frecps  v13.4s, v0.4s, v1.4s
    frecpe  v14.4s, v0.4s
    frecps  v15.4s, v0.4s, v1.4s
    frecpe  v16.4s, v0.4s
    frecps  v17.4s, v0.4s, v1.4s
    frecpe  v18.4s, v0.4s
    frecps  v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    frecpe  v20.4s, v0.4s
    frecps  v21.4s, v0.4s, v1.4s
    frecpe  v22.4s, v0.4s
    frecps  v23.4s, v0.4s, v1.4s
    frecpe  v24.4s, v0.4s
    frecps  v25.4s, v0.4s, v1.4s
    frecpe  v26.4s, v0.4s
    frecps  v27.4s, v0.4s, v1.4s
    frecpe  v28.4s, v0.4s
    frecps  v29.4s, v0.4s, v1.4s
    frecpe  v30.4s, v0.4s
    frecps  v31.4s, v0.4s, v1.4s
    bne .asimd.frecpe.recps.v.f32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_frsqrte_rsqrts_v_f32
    preserve_caller_vec
    INIT
.asimd.frsqrte.rsqrts.v.f32.L1:
    frsqrte v8.4s , v0.4s
    frsqrts v9.4s , v0.4s, v1.4s
    frsqrte v10.4s, v0.4s
    frsqrts v11.4s, v0.4s, v1.4s
    frsqrte v12.4s, v0.4s
    frsqrts v13.4s, v0.4s, v1.4s
    frsqrte v14.4s, v0.4s
    frsqrts v15.4s, v0.4s, v1.4s
    frsqrte v16.4s, v0.4s
    frsqrts v17.4s, v0.4s, v1.4s
    frsqrte v18.4s, v0.4s
    frsqrts v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    frsqrte v20.4s, v0.4s
    frsqrts v21.4s, v0.4s, v1.4s
    frsqrte v22.4s, v0.4s
    frsqrts v23.4s, v0.4s, v1.4s
    frsqrte v24.4s, v0.4s
    frsqrts v25.4s, v0.4s, v1.4s
    frsqrte v26.4s, v0.4s
    frsqrts v27.4s, v0.4s, v1.4s
    frsqrte v28.4s, v0.4s
    frsqrts v29.4s, v0.4s, v1.4s
    frsqrte v30.4s, v0.4s
    frsqrts v31.4s, v0.4s, v1.4s
    bne .asimd.frsqrte.rsqrts.v.f32.L1
    restore_caller_vec
    ret
###############################
#ifdef _ASIMD_HP_
    PROLOGUE asimd_frecpe_recps_v_f16
    preserve_caller_vec
    INIT
.asimd.frecpe.recps.v.f16.L1:
    frecpe  v8.8h , v0.8h
    frecps  v9.8h , v0.8h, v1.8h
    frecpe  v10.8h, v0.8h
    frecps  v11.8h, v0.8h, v1.8h
    frecpe  v12.8h, v0.8h
    frecps  v13.8h, v0.8h, v1.8h
    frecpe  v14.8h, v0.8h
    frecps  v15.8h, v0.8h, v1.8h
    frecpe  v16.8h, v0.8h
    frecps  v17.8h, v0.8h, v1.8h
    frecpe  v18.8h, v0.8h
    frecps  v19.8h, v0.8h, v1.8h
    subs x0, x0, #1
    frecpe  v20.8h, v0.8h
    frecps  v21.8h, v0.8h, v1.8h
    frecpe  v22.8h, v0.8h
    frecps  v23.8h, v0.8h, v1.8h
    frecpe  v24.8h, v0.8h
    frecps  v25.8h, v0.8h, v1.8h
    frecpe  v26.8h, v0.8h
    frecps  v27.8h, v0.8h, v1.8h
    frecpe  v28.8h, v0.8h
    frecps  v29.8h, v0.8h, v1.8h
    frecpe  v30.8h, v0.8h
    frecps  v31.8h, v0.8h, v1.8h
    bne .asimd.frecpe.recps.v.f16.L1
    restore_caller_vec
    ret
#endif

###############################
// Latency variants: Pattern B — chain estimate -> refine -> estimate -> ...
// all through v8. Reads out the round-trip "estimate + one refinement step"
// latency, useful for understanding how many cycles a Newton-Raphson iter
// takes end-to-end.
###############################
#void asimd_frecpe_recps_v_f32_latency(int64_t);
    PROLOGUE asimd_frecpe_recps_v_f32_latency
    preserve_caller_vec
    INIT
    fmov v0.4s, #1.0    // stable non-zero seed for the chain
    fmov v8.4s, #1.0
.asimd.frecpe.recps.v.f32.lat.L1:
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    subs x0, x0, #1
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    frecpe v8.4s, v8.4s
    frecps v8.4s, v8.4s, v0.4s
    bne .asimd.frecpe.recps.v.f32.lat.L1
    restore_caller_vec
    ret
###############################
#void asimd_frsqrte_rsqrts_v_f32_latency(int64_t);
    PROLOGUE asimd_frsqrte_rsqrts_v_f32_latency
    preserve_caller_vec
    INIT
    fmov v0.4s, #1.0
    fmov v8.4s, #1.0
.asimd.frsqrte.rsqrts.v.f32.lat.L1:
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    subs x0, x0, #1
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    frsqrte v8.4s, v8.4s
    frsqrts v8.4s, v8.4s, v0.4s
    bne .asimd.frsqrte.rsqrts.v.f32.lat.L1
    restore_caller_vec
    ret
###############################
#ifdef _ASIMD_HP_
#void asimd_frecpe_recps_v_f16_latency(int64_t);
    PROLOGUE asimd_frecpe_recps_v_f16_latency
    preserve_caller_vec
    INIT
    fmov v0.8h, #1.0
    fmov v8.8h, #1.0
.asimd.frecpe.recps.v.f16.lat.L1:
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    subs x0, x0, #1
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    frecpe v8.8h, v8.8h
    frecps v8.8h, v8.8h, v0.8h
    bne .asimd.frecpe.recps.v.f16.lat.L1
    restore_caller_vec
    ret
#endif
