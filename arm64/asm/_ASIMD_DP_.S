.align 4
#include "common_arm64.h"
#void asimd_fmla_vs_fp16fp16fp16(int64_t);
#void asimd_fmla_vv_fp16fp16fp16(int64_t);

    PROLOGUE asimd_dp4a_vs_s32s8s8
    preserve_caller_vec
    INIT
.asimd.dp4a.vs.s32s8s8.L1:
    sdot v8.4s, v0.16b, v1.4b[0]
    sdot v9.4s, v0.16b, v1.4b[0]
    sdot v10.4s, v0.16b, v1.4b[0]
    sdot v11.4s, v0.16b, v1.4b[0]
    sdot v12.4s, v0.16b, v1.4b[0]
    sdot v13.4s, v0.16b, v1.4b[0]
    sdot v14.4s, v0.16b, v1.4b[0]
    sdot v15.4s, v0.16b, v1.4b[0]
    sdot v16.4s, v0.16b, v1.4b[0]
    sdot v17.4s, v0.16b, v1.4b[0]
    sdot v18.4s, v0.16b, v1.4b[0]
    sdot v19.4s, v0.16b, v1.4b[0]
    subs x0, x0, #1
    sdot v20.4s, v0.16b, v1.4b[0]
    sdot v21.4s, v0.16b, v1.4b[0]
    sdot v22.4s, v0.16b, v1.4b[0]
    sdot v23.4s, v0.16b, v1.4b[0]
    sdot v24.4s, v0.16b, v1.4b[0]
    sdot v25.4s, v0.16b, v1.4b[0]
    sdot v26.4s, v0.16b, v1.4b[0]
    sdot v27.4s, v0.16b, v1.4b[0]
    sdot v28.4s, v0.16b, v1.4b[0]
    sdot v29.4s, v0.16b, v1.4b[0]
    sdot v30.4s, v0.16b, v1.4b[0]
    sdot v31.4s, v0.16b, v1.4b[0]
    bne .asimd.dp4a.vs.s32s8s8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp4a_vv_s32s8s8
    preserve_caller_vec
    INIT
.asimd.dp4a.vv.s32s8s8.L1:
    sdot v8.4s, v0.16b, v1.16b
    sdot v9.4s, v0.16b, v1.16b
    sdot v10.4s, v0.16b, v1.16b
    sdot v11.4s, v0.16b, v1.16b
    sdot v12.4s, v0.16b, v1.16b
    sdot v13.4s, v0.16b, v1.16b
    sdot v14.4s, v0.16b, v1.16b
    sdot v15.4s, v0.16b, v1.16b
    sdot v16.4s, v0.16b, v1.16b
    sdot v17.4s, v0.16b, v1.16b
    sdot v18.4s, v0.16b, v1.16b
    sdot v19.4s, v0.16b, v1.16b
    subs x0, x0, #1
    sdot v20.4s, v0.16b, v1.16b
    sdot v21.4s, v0.16b, v1.16b
    sdot v22.4s, v0.16b, v1.16b
    sdot v23.4s, v0.16b, v1.16b
    sdot v24.4s, v0.16b, v1.16b
    sdot v25.4s, v0.16b, v1.16b
    sdot v26.4s, v0.16b, v1.16b
    sdot v27.4s, v0.16b, v1.16b
    sdot v28.4s, v0.16b, v1.16b
    sdot v29.4s, v0.16b, v1.16b
    sdot v30.4s, v0.16b, v1.16b
    sdot v31.4s, v0.16b, v1.16b
    bne .asimd.dp4a.vv.s32s8s8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp4a_vs_u32u8u8
    preserve_caller_vec
    INIT
.asimd.dp4a.vs.u32u8u8.L1:
    udot v8.4s, v0.16b, v1.4b[0]
    udot v9.4s, v0.16b, v1.4b[0]
    udot v10.4s, v0.16b, v1.4b[0]
    udot v11.4s, v0.16b, v1.4b[0]
    udot v12.4s, v0.16b, v1.4b[0]
    udot v13.4s, v0.16b, v1.4b[0]
    udot v14.4s, v0.16b, v1.4b[0]
    udot v15.4s, v0.16b, v1.4b[0]
    udot v16.4s, v0.16b, v1.4b[0]
    udot v17.4s, v0.16b, v1.4b[0]
    udot v18.4s, v0.16b, v1.4b[0]
    udot v19.4s, v0.16b, v1.4b[0]
    subs x0, x0, #1
    udot v20.4s, v0.16b, v1.4b[0]
    udot v21.4s, v0.16b, v1.4b[0]
    udot v22.4s, v0.16b, v1.4b[0]
    udot v23.4s, v0.16b, v1.4b[0]
    udot v24.4s, v0.16b, v1.4b[0]
    udot v25.4s, v0.16b, v1.4b[0]
    udot v26.4s, v0.16b, v1.4b[0]
    udot v27.4s, v0.16b, v1.4b[0]
    udot v28.4s, v0.16b, v1.4b[0]
    udot v29.4s, v0.16b, v1.4b[0]
    udot v30.4s, v0.16b, v1.4b[0]
    udot v31.4s, v0.16b, v1.4b[0]
    bne .asimd.dp4a.vs.u32u8u8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp4a_vv_u32u8u8
    preserve_caller_vec
    INIT
.asimd.dp4a.vv.u32u8u8.L1:
    udot v8.4s, v0.16b, v1.16b
    udot v9.4s, v0.16b, v1.16b
    udot v10.4s, v0.16b, v1.16b
    udot v11.4s, v0.16b, v1.16b
    udot v12.4s, v0.16b, v1.16b
    udot v13.4s, v0.16b, v1.16b
    udot v14.4s, v0.16b, v1.16b
    udot v15.4s, v0.16b, v1.16b
    udot v16.4s, v0.16b, v1.16b
    udot v17.4s, v0.16b, v1.16b
    udot v18.4s, v0.16b, v1.16b
    udot v19.4s, v0.16b, v1.16b
    subs x0, x0, #1
    udot v20.4s, v0.16b, v1.16b
    udot v21.4s, v0.16b, v1.16b
    udot v22.4s, v0.16b, v1.16b
    udot v23.4s, v0.16b, v1.16b
    udot v24.4s, v0.16b, v1.16b
    udot v25.4s, v0.16b, v1.16b
    udot v26.4s, v0.16b, v1.16b
    udot v27.4s, v0.16b, v1.16b
    udot v28.4s, v0.16b, v1.16b
    udot v29.4s, v0.16b, v1.16b
    udot v30.4s, v0.16b, v1.16b
    udot v31.4s, v0.16b, v1.16b
    bne .asimd.dp4a.vv.u32u8u8.L1
    restore_caller_vec
    ret

