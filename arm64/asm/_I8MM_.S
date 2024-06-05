.align 4
#include "common_arm64.h"
#void asimd_mmla_s32s8s8(int64_t);
#void asimd_mmla_u32u8u8(int64_t);
#void asimd_mmla_s32u8s8(int64_t);

#void asimd_dp4a_vs_s32s8u8(int64_t);
#void asimd_dp4a_vs_s32u8s8(int64_t);
#void asimd_dp4a_vv_s32u8s8(int64_t);
    PROLOGUE asimd_mmla_s32s8s8

    preserve_caller_vec
    INIT
.asimd.mmla.s32s8s8.L1:
    smmla v8.4s, v0.16b, v1.16b
    smmla v9.4s, v0.16b, v1.16b
    smmla v10.4s, v0.16b, v1.16b
    smmla v11.4s, v0.16b, v1.16b
    smmla v12.4s, v0.16b, v1.16b
    smmla v13.4s, v0.16b, v1.16b
    smmla v14.4s, v0.16b, v1.16b
    smmla v15.4s, v0.16b, v1.16b
    smmla v16.4s, v0.16b, v1.16b
    smmla v17.4s, v0.16b, v1.16b
    smmla v18.4s, v0.16b, v1.16b
    smmla v19.4s, v0.16b, v1.16b
    subs x0, x0, #1
    smmla v20.4s, v0.16b, v1.16b
    smmla v21.4s, v0.16b, v1.16b
    smmla v22.4s, v0.16b, v1.16b
    smmla v23.4s, v0.16b, v1.16b
    smmla v24.4s, v0.16b, v1.16b
    smmla v25.4s, v0.16b, v1.16b
    smmla v26.4s, v0.16b, v1.16b
    smmla v27.4s, v0.16b, v1.16b
    smmla v28.4s, v0.16b, v1.16b
    smmla v29.4s, v0.16b, v1.16b
    smmla v30.4s, v0.16b, v1.16b
    smmla v31.4s, v0.16b, v1.16b
    bne .asimd.mmla.s32s8s8.L1
    restore_caller_vec
    ret

###############################
    PROLOGUE asimd_mmla_u32u8u8

    preserve_caller_vec
    INIT
.asimd.mmla.u32u8u8.L1:
    ummla v8.4s, v0.16b, v1.16b
    ummla v9.4s, v0.16b, v1.16b
    ummla v10.4s, v0.16b, v1.16b
    ummla v11.4s, v0.16b, v1.16b
    ummla v12.4s, v0.16b, v1.16b
    ummla v13.4s, v0.16b, v1.16b
    ummla v14.4s, v0.16b, v1.16b
    ummla v15.4s, v0.16b, v1.16b
    ummla v16.4s, v0.16b, v1.16b
    ummla v17.4s, v0.16b, v1.16b
    ummla v18.4s, v0.16b, v1.16b
    ummla v19.4s, v0.16b, v1.16b
    subs x0, x0, #1
    ummla v20.4s, v0.16b, v1.16b
    ummla v21.4s, v0.16b, v1.16b
    ummla v22.4s, v0.16b, v1.16b
    ummla v23.4s, v0.16b, v1.16b
    ummla v24.4s, v0.16b, v1.16b
    ummla v25.4s, v0.16b, v1.16b
    ummla v26.4s, v0.16b, v1.16b
    ummla v27.4s, v0.16b, v1.16b
    ummla v28.4s, v0.16b, v1.16b
    ummla v29.4s, v0.16b, v1.16b
    ummla v30.4s, v0.16b, v1.16b
    ummla v31.4s, v0.16b, v1.16b
    bne .asimd.mmla.u32u8u8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_mmla_s32u8s8

    preserve_caller_vec
    INIT
.asimd.mmla.s32u8s8.L1:
    usmmla v8.4s, v0.16b, v1.16b
    usmmla v9.4s, v0.16b, v1.16b
    usmmla v10.4s, v0.16b, v1.16b
    usmmla v11.4s, v0.16b, v1.16b
    usmmla v12.4s, v0.16b, v1.16b
    usmmla v13.4s, v0.16b, v1.16b
    usmmla v14.4s, v0.16b, v1.16b
    usmmla v15.4s, v0.16b, v1.16b
    usmmla v16.4s, v0.16b, v1.16b
    usmmla v17.4s, v0.16b, v1.16b
    usmmla v18.4s, v0.16b, v1.16b
    usmmla v19.4s, v0.16b, v1.16b
    subs x0, x0, #1
    usmmla v20.4s, v0.16b, v1.16b
    usmmla v21.4s, v0.16b, v1.16b
    usmmla v22.4s, v0.16b, v1.16b
    usmmla v23.4s, v0.16b, v1.16b
    usmmla v24.4s, v0.16b, v1.16b
    usmmla v25.4s, v0.16b, v1.16b
    usmmla v26.4s, v0.16b, v1.16b
    usmmla v27.4s, v0.16b, v1.16b
    usmmla v28.4s, v0.16b, v1.16b
    usmmla v29.4s, v0.16b, v1.16b
    usmmla v30.4s, v0.16b, v1.16b
    usmmla v31.4s, v0.16b, v1.16b
    bne .asimd.mmla.s32u8s8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp4a_vs_s32s8u8

    preserve_caller_vec
    INIT
.asimd.dp4a.vs.s32s8u8.L1:
    sudot v8.4s, v0.16b, v1.4b[0]
    sudot v9.4s, v0.16b, v1.4b[0]
    sudot v10.4s, v0.16b, v1.4b[0]
    sudot v11.4s, v0.16b, v1.4b[0]
    sudot v12.4s, v0.16b, v1.4b[0]
    sudot v13.4s, v0.16b, v1.4b[0]
    sudot v14.4s, v0.16b, v1.4b[0]
    sudot v15.4s, v0.16b, v1.4b[0]
    sudot v16.4s, v0.16b, v1.4b[0]
    sudot v17.4s, v0.16b, v1.4b[0]
    sudot v18.4s, v0.16b, v1.4b[0]
    sudot v19.4s, v0.16b, v1.4b[0]
    subs x0, x0, #1
    sudot v20.4s, v0.16b, v1.4b[0]
    sudot v21.4s, v0.16b, v1.4b[0]
    sudot v22.4s, v0.16b, v1.4b[0]
    sudot v23.4s, v0.16b, v1.4b[0]
    sudot v24.4s, v0.16b, v1.4b[0]
    sudot v25.4s, v0.16b, v1.4b[0]
    sudot v26.4s, v0.16b, v1.4b[0]
    sudot v27.4s, v0.16b, v1.4b[0]
    sudot v28.4s, v0.16b, v1.4b[0]
    sudot v29.4s, v0.16b, v1.4b[0]
    sudot v30.4s, v0.16b, v1.4b[0]
    sudot v31.4s, v0.16b, v1.4b[0]
    bne .asimd.dp4a.vs.s32s8u8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp4a_vs_s32u8s8

    preserve_caller_vec
    INIT
.asimd.dp4a.vs.s32u8s8.L1:
    usdot v8.4s, v0.16b, v1.4b[0]
    usdot v9.4s, v0.16b, v1.4b[0]
    usdot v10.4s, v0.16b, v1.4b[0]
    usdot v11.4s, v0.16b, v1.4b[0]
    usdot v12.4s, v0.16b, v1.4b[0]
    usdot v13.4s, v0.16b, v1.4b[0]
    usdot v14.4s, v0.16b, v1.4b[0]
    usdot v15.4s, v0.16b, v1.4b[0]
    usdot v16.4s, v0.16b, v1.4b[0]
    usdot v17.4s, v0.16b, v1.4b[0]
    usdot v18.4s, v0.16b, v1.4b[0]
    usdot v19.4s, v0.16b, v1.4b[0]
    subs x0, x0, #1
    usdot v20.4s, v0.16b, v1.4b[0]
    usdot v21.4s, v0.16b, v1.4b[0]
    usdot v22.4s, v0.16b, v1.4b[0]
    usdot v23.4s, v0.16b, v1.4b[0]
    usdot v24.4s, v0.16b, v1.4b[0]
    usdot v25.4s, v0.16b, v1.4b[0]
    usdot v26.4s, v0.16b, v1.4b[0]
    usdot v27.4s, v0.16b, v1.4b[0]
    usdot v28.4s, v0.16b, v1.4b[0]
    usdot v29.4s, v0.16b, v1.4b[0]
    usdot v30.4s, v0.16b, v1.4b[0]
    usdot v31.4s, v0.16b, v1.4b[0]
    bne .asimd.dp4a.vs.s32u8s8.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_dp4a_vv_s32u8s8

    preserve_caller_vec
    INIT
.asimd.dp4a.vv.s32u8s8.L1:
    usdot v8.4s, v0.16b, v1.16b
    usdot v9.4s, v0.16b, v1.16b
    usdot v10.4s, v0.16b, v1.16b
    usdot v11.4s, v0.16b, v1.16b
    usdot v12.4s, v0.16b, v1.16b
    usdot v13.4s, v0.16b, v1.16b
    usdot v14.4s, v0.16b, v1.16b
    usdot v15.4s, v0.16b, v1.16b
    usdot v16.4s, v0.16b, v1.16b
    usdot v17.4s, v0.16b, v1.16b
    usdot v18.4s, v0.16b, v1.16b
    usdot v19.4s, v0.16b, v1.16b
    subs x0, x0, #1
    usdot v20.4s, v0.16b, v1.16b
    usdot v21.4s, v0.16b, v1.16b
    usdot v22.4s, v0.16b, v1.16b
    usdot v23.4s, v0.16b, v1.16b
    usdot v24.4s, v0.16b, v1.16b
    usdot v25.4s, v0.16b, v1.16b
    usdot v26.4s, v0.16b, v1.16b
    usdot v27.4s, v0.16b, v1.16b
    usdot v28.4s, v0.16b, v1.16b
    usdot v29.4s, v0.16b, v1.16b
    usdot v30.4s, v0.16b, v1.16b
    usdot v31.4s, v0.16b, v1.16b
    bne .asimd.dp4a.vv.s32u8s8.L1
    restore_caller_vec
    ret

