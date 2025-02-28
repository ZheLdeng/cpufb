.align 4
#include "common_arm64.h"
#void asimd_fmla_vs_f32f32f32(int64_t);
#void asimd_fmla_vv_f32f32f32(int64_t);
#void asimd_fmla_vs_f64f64f64(int64_t);
#void asimd_fmla_vv_f64f64f64(int64_t);

    PROLOGUE asimd_fmla_vs_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fmla.vs.f32f32f32.L1:
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v9.4s, v0.4s, v1.s[0]
    fmla v10.4s, v0.4s, v1.s[0]
    fmla v11.4s, v0.4s, v1.s[0]
    fmla v12.4s, v0.4s, v1.s[0]
    fmla v13.4s, v0.4s, v1.s[0]
    fmla v14.4s, v0.4s, v1.s[0]
    fmla v15.4s, v0.4s, v1.s[0]
    fmla v16.4s, v0.4s, v1.s[0]
    fmla v17.4s, v0.4s, v1.s[0]
    fmla v18.4s, v0.4s, v1.s[0]
    fmla v19.4s, v0.4s, v1.s[0]
    subs x0, x0, #1
    fmla v20.4s, v0.4s, v1.s[0]
    fmla v21.4s, v0.4s, v1.s[0]
    fmla v22.4s, v0.4s, v1.s[0]
    fmla v23.4s, v0.4s, v1.s[0]
    fmla v24.4s, v0.4s, v1.s[0]
    fmla v25.4s, v0.4s, v1.s[0]
    fmla v26.4s, v0.4s, v1.s[0]
    fmla v27.4s, v0.4s, v1.s[0]
    fmla v28.4s, v0.4s, v1.s[0]
    fmla v29.4s, v0.4s, v1.s[0]
    fmla v30.4s, v0.4s, v1.s[0]
    fmla v31.4s, v0.4s, v1.s[0]
    bne .asimd.fmla.vs.f32f32f32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmla_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fmla.vv.f32f32f32.L1:
    fmla v8.4s, v0.4s, v1.4s
    fmla v9.4s, v0.4s, v1.4s
    fmla v10.4s, v0.4s, v1.4s
    fmla v11.4s, v0.4s, v1.4s
    fmla v12.4s, v0.4s, v1.4s
    fmla v13.4s, v0.4s, v1.4s
    fmla v14.4s, v0.4s, v1.4s
    fmla v15.4s, v0.4s, v1.4s
    fmla v16.4s, v0.4s, v1.4s
    fmla v17.4s, v0.4s, v1.4s
    fmla v18.4s, v0.4s, v1.4s
    fmla v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    fmla v20.4s, v0.4s, v1.4s
    fmla v21.4s, v0.4s, v1.4s
    fmla v22.4s, v0.4s, v1.4s
    fmla v23.4s, v0.4s, v1.4s
    fmla v24.4s, v0.4s, v1.4s
    fmla v25.4s, v0.4s, v1.4s
    fmla v26.4s, v0.4s, v1.4s
    fmla v27.4s, v0.4s, v1.4s
    fmla v28.4s, v0.4s, v1.4s
    fmla v29.4s, v0.4s, v1.4s
    fmla v30.4s, v0.4s, v1.4s
    fmla v31.4s, v0.4s, v1.4s
    bne .asimd.fmla.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmla_vs_f64f64f64
    preserve_caller_vec
    INIT
.asimd.fmla.vs.f64f64f64.L1:
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v9.2d, v0.2d, v1.d[0]
    fmla v10.2d, v0.2d, v1.d[0]
    fmla v11.2d, v0.2d, v1.d[0]
    fmla v12.2d, v0.2d, v1.d[0]
    fmla v13.2d, v0.2d, v1.d[0]
    fmla v14.2d, v0.2d, v1.d[0]
    fmla v15.2d, v0.2d, v1.d[0]
    fmla v16.2d, v0.2d, v1.d[0]
    fmla v17.2d, v0.2d, v1.d[0]
    fmla v18.2d, v0.2d, v1.d[0]
    fmla v19.2d, v0.2d, v1.d[0]
    subs x0, x0, #1
    fmla v20.2d, v0.2d, v1.d[0]
    fmla v21.2d, v0.2d, v1.d[0]
    fmla v22.2d, v0.2d, v1.d[0]
    fmla v23.2d, v0.2d, v1.d[0]
    fmla v24.2d, v0.2d, v1.d[0]
    fmla v25.2d, v0.2d, v1.d[0]
    fmla v26.2d, v0.2d, v1.d[0]
    fmla v27.2d, v0.2d, v1.d[0]
    fmla v28.2d, v0.2d, v1.d[0]
    fmla v29.2d, v0.2d, v1.d[0]
    fmla v30.2d, v0.2d, v1.d[0]
    fmla v31.2d, v0.2d, v1.d[0]
    bne .asimd.fmla.vs.f64f64f64.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmla_vv_f64f64f64
    preserve_caller_vec
    INIT
.asimd.fmla.vv.f64f64f64.L1:
    fmla v8.2d, v0.2d, v1.2d
    fmla v9.2d, v0.2d, v1.2d
    fmla v10.2d, v0.2d, v1.2d
    fmla v11.2d, v0.2d, v1.2d
    fmla v12.2d, v0.2d, v1.2d
    fmla v13.2d, v0.2d, v1.2d
    fmla v14.2d, v0.2d, v1.2d
    fmla v15.2d, v0.2d, v1.2d
    fmla v16.2d, v0.2d, v1.2d
    fmla v17.2d, v0.2d, v1.2d
    fmla v18.2d, v0.2d, v1.2d
    fmla v19.2d, v0.2d, v1.2d
    subs x0, x0, #1
    fmla v20.2d, v0.2d, v1.2d
    fmla v21.2d, v0.2d, v1.2d
    fmla v22.2d, v0.2d, v1.2d
    fmla v23.2d, v0.2d, v1.2d
    fmla v24.2d, v0.2d, v1.2d
    fmla v25.2d, v0.2d, v1.2d
    fmla v26.2d, v0.2d, v1.2d
    fmla v27.2d, v0.2d, v1.2d
    fmla v28.2d, v0.2d, v1.2d
    fmla v29.2d, v0.2d, v1.2d
    fmla v30.2d, v0.2d, v1.2d
    fmla v31.2d, v0.2d, v1.2d
    bne .asimd.fmla.vv.f64f64f64.L1
    restore_caller_vec
    ret

 ###############################   
    PROLOGUE asimd_fmla2_vs_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fmla2.vs.f32f32f32.L1:
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    subs x0, x0, #1
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    fmla v8.4s, v0.4s, v1.s[0]
    bne .asimd.fmla2.vs.f32f32f32.L1
    restore_caller_vec
    ret
###############################
