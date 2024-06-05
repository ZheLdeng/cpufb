.align 4
#include "common_arm64.h"
#void asimd_dp4a_vs_s32s8s8(int64_t);
#void asimd_dp4a_vv_s32s8s8(int64_t);
#void asimd_dp4a_vs_u32u8u8(int64_t);
#void asimd_dp4a_vv_u32u8u8(int64_t);

    PROLOGUE asimd_fmla_vs_fp16fp16fp16
    preserve_caller_vec
    INIT
.asimd.fmla.vs.fp16fp16fp16.L1:
    fmla v8.8h, v0.8h, v1.h[0]
    fmla v9.8h, v0.8h, v1.h[0]
    fmla v10.8h, v0.8h, v1.h[0]
    fmla v11.8h, v0.8h, v1.h[0]
    fmla v12.8h, v0.8h, v1.h[0]
    fmla v13.8h, v0.8h, v1.h[0]
    fmla v14.8h, v0.8h, v1.h[0]
    fmla v15.8h, v0.8h, v1.h[0]
    fmla v16.8h, v0.8h, v1.h[0]
    fmla v17.8h, v0.8h, v1.h[0]
    fmla v18.8h, v0.8h, v1.h[0]
    fmla v19.8h, v0.8h, v1.h[0]
    subs x0, x0, #1
    fmla v20.8h, v0.8h, v1.h[0]
    fmla v21.8h, v0.8h, v1.h[0]
    fmla v22.8h, v0.8h, v1.h[0]
    fmla v23.8h, v0.8h, v1.h[0]
    fmla v24.8h, v0.8h, v1.h[0]
    fmla v25.8h, v0.8h, v1.h[0]
    fmla v26.8h, v0.8h, v1.h[0]
    fmla v27.8h, v0.8h, v1.h[0]
    fmla v28.8h, v0.8h, v1.h[0]
    fmla v29.8h, v0.8h, v1.h[0]
    fmla v30.8h, v0.8h, v1.h[0]
    fmla v31.8h, v0.8h, v1.h[0]
    bne .asimd.fmla.vs.fp16fp16fp16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmla_vv_fp16fp16fp16
    preserve_caller_vec
    INIT
.asimd.fmla.vv.fp16fp16fp16.L1:
    fmla v8.8h, v0.8h, v1.8h
    fmla v9.8h, v0.8h, v1.8h
    fmla v10.8h, v0.8h, v1.8h
    fmla v11.8h, v0.8h, v1.8h
    fmla v12.8h, v0.8h, v1.8h
    fmla v13.8h, v0.8h, v1.8h
    fmla v14.8h, v0.8h, v1.8h
    fmla v15.8h, v0.8h, v1.8h
    fmla v16.8h, v0.8h, v1.8h
    fmla v17.8h, v0.8h, v1.8h
    fmla v18.8h, v0.8h, v1.8h
    fmla v19.8h, v0.8h, v1.8h
    subs x0, x0, #1
    fmla v20.8h, v0.8h, v1.8h
    fmla v21.8h, v0.8h, v1.8h
    fmla v22.8h, v0.8h, v1.8h
    fmla v23.8h, v0.8h, v1.8h
    fmla v24.8h, v0.8h, v1.8h
    fmla v25.8h, v0.8h, v1.8h
    fmla v26.8h, v0.8h, v1.8h
    fmla v27.8h, v0.8h, v1.8h
    fmla v28.8h, v0.8h, v1.8h
    fmla v29.8h, v0.8h, v1.8h
    fmla v30.8h, v0.8h, v1.8h
    fmla v31.8h, v0.8h, v1.8h
    bne .asimd.fmla.vv.fp16fp16fp16.L1
    restore_caller_vec
    ret

