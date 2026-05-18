.align 4
#include "common_arm64.h"
#void asimd_mmla_fp32bf16bf16(int64_t);
#void asimd_dp2a_vs_fp32bf16bf16(int64_t);
#void asimd_dp2a_vv_fp32bf16bf16(int64_t);
#void asimd_bfmlalb_fp32bf16bf16(int64_t);
#void asimd_bfmlalt_fp32bf16bf16(int64_t);
    PROLOGUE asimd_mmla_fp32bf16bf16

    preserve_caller_vec
    INIT
.asimd.mmla.fp32bf16bf16.L1:
    bfmmla v8.4s, v0.8h, v1.8h
    bfmmla v9.4s, v0.8h, v1.8h
    bfmmla v10.4s, v0.8h, v1.8h
    bfmmla v11.4s, v0.8h, v1.8h
    bfmmla v12.4s, v0.8h, v1.8h
    bfmmla v13.4s, v0.8h, v1.8h
    bfmmla v14.4s, v0.8h, v1.8h
    bfmmla v15.4s, v0.8h, v1.8h
    bfmmla v16.4s, v0.8h, v1.8h
    bfmmla v17.4s, v0.8h, v1.8h
    bfmmla v18.4s, v0.8h, v1.8h
    bfmmla v19.4s, v0.8h, v1.8h
    subs x0, x0, #1
    bfmmla v20.4s, v0.8h, v1.8h
    bfmmla v21.4s, v0.8h, v1.8h
    bfmmla v22.4s, v0.8h, v1.8h
    bfmmla v23.4s, v0.8h, v1.8h
    bfmmla v24.4s, v0.8h, v1.8h
    bfmmla v25.4s, v0.8h, v1.8h
    bfmmla v26.4s, v0.8h, v1.8h
    bfmmla v27.4s, v0.8h, v1.8h
    bfmmla v28.4s, v0.8h, v1.8h
    bfmmla v29.4s, v0.8h, v1.8h
    bfmmla v30.4s, v0.8h, v1.8h
    bfmmla v31.4s, v0.8h, v1.8h
    bne .asimd.mmla.fp32bf16bf16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp2a_vs_fp32bf16bf16
    preserve_caller_vec
    INIT
.asimd.dp2a.vs.fp32bf16bf16.L1:
    bfdot v8.4s, v0.8h, v1.2h[0]
    bfdot v9.4s, v0.8h, v1.2h[0]
    bfdot v10.4s, v0.8h, v1.2h[0]
    bfdot v11.4s, v0.8h, v1.2h[0]
    bfdot v12.4s, v0.8h, v1.2h[0]
    bfdot v13.4s, v0.8h, v1.2h[0]
    bfdot v14.4s, v0.8h, v1.2h[0]
    bfdot v15.4s, v0.8h, v1.2h[0]
    bfdot v16.4s, v0.8h, v1.2h[0]
    bfdot v17.4s, v0.8h, v1.2h[0]
    bfdot v18.4s, v0.8h, v1.2h[0]
    bfdot v19.4s, v0.8h, v1.2h[0]
    subs x0, x0, #1
    bfdot v20.4s, v0.8h, v1.2h[0]
    bfdot v21.4s, v0.8h, v1.2h[0]
    bfdot v22.4s, v0.8h, v1.2h[0]
    bfdot v23.4s, v0.8h, v1.2h[0]
    bfdot v24.4s, v0.8h, v1.2h[0]
    bfdot v25.4s, v0.8h, v1.2h[0]
    bfdot v26.4s, v0.8h, v1.2h[0]
    bfdot v27.4s, v0.8h, v1.2h[0]
    bfdot v28.4s, v0.8h, v1.2h[0]
    bfdot v29.4s, v0.8h, v1.2h[0]
    bfdot v30.4s, v0.8h, v1.2h[0]
    bfdot v31.4s, v0.8h, v1.2h[0]
    bne .asimd.dp2a.vs.fp32bf16bf16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp2a_vv_fp32bf16bf16
    preserve_caller_vec
    INIT
.asimd.dp2a.vv.fp32bf16bf16.L1:
    bfdot v8.4s, v0.8h, v1.8h
    bfdot v9.4s, v0.8h, v1.8h
    bfdot v10.4s, v0.8h, v1.8h
    bfdot v11.4s, v0.8h, v1.8h
    bfdot v12.4s, v0.8h, v1.8h
    bfdot v13.4s, v0.8h, v1.8h
    bfdot v14.4s, v0.8h, v1.8h
    bfdot v15.4s, v0.8h, v1.8h
    bfdot v16.4s, v0.8h, v1.8h
    bfdot v17.4s, v0.8h, v1.8h
    bfdot v18.4s, v0.8h, v1.8h
    bfdot v19.4s, v0.8h, v1.8h
    subs x0, x0, #1
    bfdot v20.4s, v0.8h, v1.8h
    bfdot v21.4s, v0.8h, v1.8h
    bfdot v22.4s, v0.8h, v1.8h
    bfdot v23.4s, v0.8h, v1.8h
    bfdot v24.4s, v0.8h, v1.8h
    bfdot v25.4s, v0.8h, v1.8h
    bfdot v26.4s, v0.8h, v1.8h
    bfdot v27.4s, v0.8h, v1.8h
    bfdot v28.4s, v0.8h, v1.8h
    bfdot v29.4s, v0.8h, v1.8h
    bfdot v30.4s, v0.8h, v1.8h
    bfdot v31.4s, v0.8h, v1.8h
    bne .asimd.dp2a.vv.fp32bf16bf16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_bfmlalb_fp32bf16bf16
    preserve_caller_vec
    INIT
.asimd.bfmlalb.fp32bf16bf16.L1:
    bfmlalb v8.4s, v0.8h, v1.8h
    bfmlalb v9.4s, v0.8h, v1.8h
    bfmlalb v10.4s, v0.8h, v1.8h
    bfmlalb v11.4s, v0.8h, v1.8h
    bfmlalb v12.4s, v0.8h, v1.8h
    bfmlalb v13.4s, v0.8h, v1.8h
    bfmlalb v14.4s, v0.8h, v1.8h
    bfmlalb v15.4s, v0.8h, v1.8h
    bfmlalb v16.4s, v0.8h, v1.8h
    bfmlalb v17.4s, v0.8h, v1.8h
    bfmlalb v18.4s, v0.8h, v1.8h
    bfmlalb v19.4s, v0.8h, v1.8h
    subs x0, x0, #1
    bfmlalb v20.4s, v0.8h, v1.8h
    bfmlalb v21.4s, v0.8h, v1.8h
    bfmlalb v22.4s, v0.8h, v1.8h
    bfmlalb v23.4s, v0.8h, v1.8h
    bfmlalb v24.4s, v0.8h, v1.8h
    bfmlalb v25.4s, v0.8h, v1.8h
    bfmlalb v26.4s, v0.8h, v1.8h
    bfmlalb v27.4s, v0.8h, v1.8h
    bfmlalb v28.4s, v0.8h, v1.8h
    bfmlalb v29.4s, v0.8h, v1.8h
    bfmlalb v30.4s, v0.8h, v1.8h
    bfmlalb v31.4s, v0.8h, v1.8h
    bne .asimd.bfmlalb.fp32bf16bf16.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_bfmlalt_fp32bf16bf16
    preserve_caller_vec
    INIT
.asimd.bfmlalt.fp32bf16bf16.L1:
    bfmlalt v8.4s, v0.8h, v1.8h
    bfmlalt v9.4s, v0.8h, v1.8h
    bfmlalt v10.4s, v0.8h, v1.8h
    bfmlalt v11.4s, v0.8h, v1.8h
    bfmlalt v12.4s, v0.8h, v1.8h
    bfmlalt v13.4s, v0.8h, v1.8h
    bfmlalt v14.4s, v0.8h, v1.8h
    bfmlalt v15.4s, v0.8h, v1.8h
    bfmlalt v16.4s, v0.8h, v1.8h
    bfmlalt v17.4s, v0.8h, v1.8h
    bfmlalt v18.4s, v0.8h, v1.8h
    bfmlalt v19.4s, v0.8h, v1.8h
    subs x0, x0, #1
    bfmlalt v20.4s, v0.8h, v1.8h
    bfmlalt v21.4s, v0.8h, v1.8h
    bfmlalt v22.4s, v0.8h, v1.8h
    bfmlalt v23.4s, v0.8h, v1.8h
    bfmlalt v24.4s, v0.8h, v1.8h
    bfmlalt v25.4s, v0.8h, v1.8h
    bfmlalt v26.4s, v0.8h, v1.8h
    bfmlalt v27.4s, v0.8h, v1.8h
    bfmlalt v28.4s, v0.8h, v1.8h
    bfmlalt v29.4s, v0.8h, v1.8h
    bfmlalt v30.4s, v0.8h, v1.8h
    bfmlalt v31.4s, v0.8h, v1.8h
    bne .asimd.bfmlalt.fp32bf16bf16.L1
    restore_caller_vec
    ret
