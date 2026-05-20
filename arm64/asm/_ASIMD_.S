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
    PROLOGUE asimd_fmla2_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fmla2.vv.f32f32f32.L1:
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    subs x0, x0, #1
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    fmla v8.4s, v0.4s, v1.4s
    bne .asimd.fmla2.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmla2_vs_f64f64f64
    preserve_caller_vec
    INIT
.asimd.fmla2.vs.f64f64f64.L1:
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    subs x0, x0, #1
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    fmla v8.2d, v0.2d, v1.d[0]
    bne .asimd.fmla2.vs.f64f64f64.L1
    restore_caller_vec
    ret
###############################
    PROLOGUE asimd_fmla2_vv_f64f64f64
    preserve_caller_vec
    INIT
.asimd.fmla2.vv.f64f64f64.L1:
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    subs x0, x0, #1
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    fmla v8.2d, v0.2d, v1.2d
    bne .asimd.fmla2.vv.f64f64f64.L1
    restore_caller_vec
    ret

 ###############################
#void asimd_hybrid_fp32_mla_6x16(int64_t);
# 仿照 ACL a64_hybrid_fp32_mla_6x16 主循环的寄存器使用：
#   v0-v5  : A 矩阵 6 行（每行 4 个 fp32，使用 .s[0..3] 作为 lane 广播源）
#   v6, v7 : B 矩阵 2 个 q 寄存器交替持有 8 列 fp32
#   v8-v31 : 6x4 = 24 个累加器（M=6, N=16, 每行 4 个 .4s 累加器）
#       行0: v8 ,v9 ,v10,v11
#       行1: v12,v13,v14,v15
#       行2: v16,v17,v18,v19
#       行3: v20,v21,v22,v23
#       行4: v24,v25,v26,v27
#       行5: v28,v29,v30,v31
# 每次循环 K=4 全展开 -> 共 96 条 4s fmla
# 单条 4s fmla = 4 lane * 2 ops = 8 FLOPS  ==> 每次循环 768 FLOPS
    PROLOGUE asimd_hybrid_fp32_mla_6x16
    preserve_caller_vec
    INIT
    eor v2.16b, v2.16b, v2.16b
    eor v3.16b, v3.16b, v3.16b
    eor v4.16b, v4.16b, v4.16b
    eor v5.16b, v5.16b, v5.16b
    eor v6.16b, v6.16b, v6.16b
    eor v7.16b, v7.16b, v7.16b
.asimd.hybrid.fp32.mla.6x16.L1:
    // ---- K lane 0 ----
    fmla v8.4s , v6.4s, v0.s[0]
    fmla v12.4s, v6.4s, v1.s[0]
    fmla v16.4s, v6.4s, v2.s[0]
    fmla v20.4s, v6.4s, v3.s[0]
    fmla v24.4s, v6.4s, v4.s[0]
    fmla v28.4s, v6.4s, v5.s[0]
    fmla v9.4s , v7.4s, v0.s[0]
    fmla v13.4s, v7.4s, v1.s[0]
    fmla v17.4s, v7.4s, v2.s[0]
    fmla v21.4s, v7.4s, v3.s[0]
    fmla v25.4s, v7.4s, v4.s[0]
    fmla v29.4s, v7.4s, v5.s[0]
    fmla v10.4s, v6.4s, v0.s[0]
    fmla v14.4s, v6.4s, v1.s[0]
    fmla v18.4s, v6.4s, v2.s[0]
    fmla v22.4s, v6.4s, v3.s[0]
    fmla v26.4s, v6.4s, v4.s[0]
    fmla v30.4s, v6.4s, v5.s[0]
    fmla v11.4s, v7.4s, v0.s[0]
    fmla v15.4s, v7.4s, v1.s[0]
    fmla v19.4s, v7.4s, v2.s[0]
    fmla v23.4s, v7.4s, v3.s[0]
    fmla v27.4s, v7.4s, v4.s[0]
    fmla v31.4s, v7.4s, v5.s[0]
    // ---- K lane 1 ----
    fmla v8.4s , v6.4s, v0.s[1]
    fmla v12.4s, v6.4s, v1.s[1]
    fmla v16.4s, v6.4s, v2.s[1]
    fmla v20.4s, v6.4s, v3.s[1]
    fmla v24.4s, v6.4s, v4.s[1]
    fmla v28.4s, v6.4s, v5.s[1]
    fmla v9.4s , v7.4s, v0.s[1]
    fmla v13.4s, v7.4s, v1.s[1]
    fmla v17.4s, v7.4s, v2.s[1]
    fmla v21.4s, v7.4s, v3.s[1]
    fmla v25.4s, v7.4s, v4.s[1]
    fmla v29.4s, v7.4s, v5.s[1]
    fmla v10.4s, v6.4s, v0.s[1]
    fmla v14.4s, v6.4s, v1.s[1]
    fmla v18.4s, v6.4s, v2.s[1]
    fmla v22.4s, v6.4s, v3.s[1]
    fmla v26.4s, v6.4s, v4.s[1]
    fmla v30.4s, v6.4s, v5.s[1]
    fmla v11.4s, v7.4s, v0.s[1]
    fmla v15.4s, v7.4s, v1.s[1]
    fmla v19.4s, v7.4s, v2.s[1]
    fmla v23.4s, v7.4s, v3.s[1]
    fmla v27.4s, v7.4s, v4.s[1]
    fmla v31.4s, v7.4s, v5.s[1]
    subs x0, x0, #1
    // ---- K lane 2 ----
    fmla v8.4s , v6.4s, v0.s[2]
    fmla v12.4s, v6.4s, v1.s[2]
    fmla v16.4s, v6.4s, v2.s[2]
    fmla v20.4s, v6.4s, v3.s[2]
    fmla v24.4s, v6.4s, v4.s[2]
    fmla v28.4s, v6.4s, v5.s[2]
    fmla v9.4s , v7.4s, v0.s[2]
    fmla v13.4s, v7.4s, v1.s[2]
    fmla v17.4s, v7.4s, v2.s[2]
    fmla v21.4s, v7.4s, v3.s[2]
    fmla v25.4s, v7.4s, v4.s[2]
    fmla v29.4s, v7.4s, v5.s[2]
    fmla v10.4s, v6.4s, v0.s[2]
    fmla v14.4s, v6.4s, v1.s[2]
    fmla v18.4s, v6.4s, v2.s[2]
    fmla v22.4s, v6.4s, v3.s[2]
    fmla v26.4s, v6.4s, v4.s[2]
    fmla v30.4s, v6.4s, v5.s[2]
    fmla v11.4s, v7.4s, v0.s[2]
    fmla v15.4s, v7.4s, v1.s[2]
    fmla v19.4s, v7.4s, v2.s[2]
    fmla v23.4s, v7.4s, v3.s[2]
    fmla v27.4s, v7.4s, v4.s[2]
    fmla v31.4s, v7.4s, v5.s[2]
    // ---- K lane 3 ----
    fmla v8.4s , v6.4s, v0.s[3]
    fmla v12.4s, v6.4s, v1.s[3]
    fmla v16.4s, v6.4s, v2.s[3]
    fmla v20.4s, v6.4s, v3.s[3]
    fmla v24.4s, v6.4s, v4.s[3]
    fmla v28.4s, v6.4s, v5.s[3]
    fmla v9.4s , v7.4s, v0.s[3]
    fmla v13.4s, v7.4s, v1.s[3]
    fmla v17.4s, v7.4s, v2.s[3]
    fmla v21.4s, v7.4s, v3.s[3]
    fmla v25.4s, v7.4s, v4.s[3]
    fmla v29.4s, v7.4s, v5.s[3]
    fmla v10.4s, v6.4s, v0.s[3]
    fmla v14.4s, v6.4s, v1.s[3]
    fmla v18.4s, v6.4s, v2.s[3]
    fmla v22.4s, v6.4s, v3.s[3]
    fmla v26.4s, v6.4s, v4.s[3]
    fmla v30.4s, v6.4s, v5.s[3]
    fmla v11.4s, v7.4s, v0.s[3]
    fmla v15.4s, v7.4s, v1.s[3]
    fmla v19.4s, v7.4s, v2.s[3]
    fmla v23.4s, v7.4s, v3.s[3]
    fmla v27.4s, v7.4s, v4.s[3]
    fmla v31.4s, v7.4s, v5.s[3]
    bne .asimd.hybrid.fp32.mla.6x16.L1
    restore_caller_vec
    ret
###############################
// fmls.vv : fused negate-multiply-add. Same op count as fmla (4 lane * 2 op).
#void asimd_fmls_vv_f32f32f32(int64_t);
    PROLOGUE asimd_fmls_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fmls.vv.f32f32f32.L1:
    fmls v8.4s , v0.4s, v1.4s
    fmls v9.4s , v0.4s, v1.4s
    fmls v10.4s, v0.4s, v1.4s
    fmls v11.4s, v0.4s, v1.4s
    fmls v12.4s, v0.4s, v1.4s
    fmls v13.4s, v0.4s, v1.4s
    fmls v14.4s, v0.4s, v1.4s
    fmls v15.4s, v0.4s, v1.4s
    fmls v16.4s, v0.4s, v1.4s
    fmls v17.4s, v0.4s, v1.4s
    fmls v18.4s, v0.4s, v1.4s
    fmls v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    fmls v20.4s, v0.4s, v1.4s
    fmls v21.4s, v0.4s, v1.4s
    fmls v22.4s, v0.4s, v1.4s
    fmls v23.4s, v0.4s, v1.4s
    fmls v24.4s, v0.4s, v1.4s
    fmls v25.4s, v0.4s, v1.4s
    fmls v26.4s, v0.4s, v1.4s
    fmls v27.4s, v0.4s, v1.4s
    fmls v28.4s, v0.4s, v1.4s
    fmls v29.4s, v0.4s, v1.4s
    fmls v30.4s, v0.4s, v1.4s
    fmls v31.4s, v0.4s, v1.4s
    bne .asimd.fmls.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
// fneg + fmla : exposes whether fneg shares the issue port with fmla.
// Pair: fneg vN, v0 ; fmla vAcc, vN, v1  (12 pairs = 24 instructions).
#void asimd_fneg_fmla_vv_f32f32f32(int64_t);
    PROLOGUE asimd_fneg_fmla_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fneg.fmla.vv.f32f32f32.L1:
    fneg v2.4s,  v0.4s
    fmla v8.4s , v2.4s, v1.4s
    fneg v3.4s,  v0.4s
    fmla v9.4s , v3.4s, v1.4s
    fneg v4.4s,  v0.4s
    fmla v10.4s, v4.4s, v1.4s
    fneg v5.4s,  v0.4s
    fmla v11.4s, v5.4s, v1.4s
    fneg v6.4s,  v0.4s
    fmla v12.4s, v6.4s, v1.4s
    fneg v7.4s,  v0.4s
    fmla v13.4s, v7.4s, v1.4s
    subs x0, x0, #1
    fneg v2.4s,  v0.4s
    fmla v14.4s, v2.4s, v1.4s
    fneg v3.4s,  v0.4s
    fmla v15.4s, v3.4s, v1.4s
    fneg v4.4s,  v0.4s
    fmla v16.4s, v4.4s, v1.4s
    fneg v5.4s,  v0.4s
    fmla v17.4s, v5.4s, v1.4s
    fneg v6.4s,  v0.4s
    fmla v18.4s, v6.4s, v1.4s
    fneg v7.4s,  v0.4s
    fmla v19.4s, v7.4s, v1.4s
    bne .asimd.fneg.fmla.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
// fadd : non-FMA add. 4 lane * 1 op = 4 FLOPS / instr.
#void asimd_fadd_vv_f32f32f32(int64_t);
    PROLOGUE asimd_fadd_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fadd.vv.f32f32f32.L1:
    fadd v8.4s , v0.4s, v1.4s
    fadd v9.4s , v0.4s, v1.4s
    fadd v10.4s, v0.4s, v1.4s
    fadd v11.4s, v0.4s, v1.4s
    fadd v12.4s, v0.4s, v1.4s
    fadd v13.4s, v0.4s, v1.4s
    fadd v14.4s, v0.4s, v1.4s
    fadd v15.4s, v0.4s, v1.4s
    fadd v16.4s, v0.4s, v1.4s
    fadd v17.4s, v0.4s, v1.4s
    fadd v18.4s, v0.4s, v1.4s
    fadd v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    fadd v20.4s, v0.4s, v1.4s
    fadd v21.4s, v0.4s, v1.4s
    fadd v22.4s, v0.4s, v1.4s
    fadd v23.4s, v0.4s, v1.4s
    fadd v24.4s, v0.4s, v1.4s
    fadd v25.4s, v0.4s, v1.4s
    fadd v26.4s, v0.4s, v1.4s
    fadd v27.4s, v0.4s, v1.4s
    fadd v28.4s, v0.4s, v1.4s
    fadd v29.4s, v0.4s, v1.4s
    fadd v30.4s, v0.4s, v1.4s
    fadd v31.4s, v0.4s, v1.4s
    bne .asimd.fadd.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
// fmul : non-FMA multiply. 4 lane * 1 op = 4 FLOPS / instr.
#void asimd_fmul_vv_f32f32f32(int64_t);
    PROLOGUE asimd_fmul_vv_f32f32f32
    preserve_caller_vec
    INIT
.asimd.fmul.vv.f32f32f32.L1:
    fmul v8.4s , v0.4s, v1.4s
    fmul v9.4s , v0.4s, v1.4s
    fmul v10.4s, v0.4s, v1.4s
    fmul v11.4s, v0.4s, v1.4s
    fmul v12.4s, v0.4s, v1.4s
    fmul v13.4s, v0.4s, v1.4s
    fmul v14.4s, v0.4s, v1.4s
    fmul v15.4s, v0.4s, v1.4s
    fmul v16.4s, v0.4s, v1.4s
    fmul v17.4s, v0.4s, v1.4s
    fmul v18.4s, v0.4s, v1.4s
    fmul v19.4s, v0.4s, v1.4s
    subs x0, x0, #1
    fmul v20.4s, v0.4s, v1.4s
    fmul v21.4s, v0.4s, v1.4s
    fmul v22.4s, v0.4s, v1.4s
    fmul v23.4s, v0.4s, v1.4s
    fmul v24.4s, v0.4s, v1.4s
    fmul v25.4s, v0.4s, v1.4s
    fmul v26.4s, v0.4s, v1.4s
    fmul v27.4s, v0.4s, v1.4s
    fmul v28.4s, v0.4s, v1.4s
    fmul v29.4s, v0.4s, v1.4s
    fmul v30.4s, v0.4s, v1.4s
    fmul v31.4s, v0.4s, v1.4s
    bne .asimd.fmul.vv.f32f32f32.L1
    restore_caller_vec
    ret
###############################
// Tier-2 latency variants.
// fmls: Pattern A (vd is RMW accumulator).
#void asimd_fmls_vv_f32f32f32_latency(int64_t);
    PROLOGUE asimd_fmls_vv_f32f32f32_latency
    preserve_caller_vec
    INIT
.asimd.fmls.vv.f32f32f32.lat.L1:
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    subs x0, x0, #1
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    fmls v8.4s, v0.4s, v1.4s
    bne .asimd.fmls.vv.f32f32f32.lat.L1
    restore_caller_vec
    ret
###############################
// fneg+fmla: Pattern B (combo chain through v8).
#void asimd_fneg_fmla_vv_f32f32f32_latency(int64_t);
    PROLOGUE asimd_fneg_fmla_vv_f32f32f32_latency
    preserve_caller_vec
    INIT
.asimd.fneg.fmla.vv.f32f32f32.lat.L1:
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    subs x0, x0, #1
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    fneg v8.4s, v8.4s
    fmla v8.4s, v8.4s, v0.4s
    bne .asimd.fneg.fmla.vv.f32f32f32.lat.L1
    restore_caller_vec
    ret
###############################
// fadd: Pattern A' (vd is not RMW; feed vd back into vn to force RAW chain).
#void asimd_fadd_vv_f32f32f32_latency(int64_t);
    PROLOGUE asimd_fadd_vv_f32f32f32_latency
    preserve_caller_vec
    INIT
.asimd.fadd.vv.f32f32f32.lat.L1:
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    subs x0, x0, #1
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    fadd v8.4s, v8.4s, v0.4s
    bne .asimd.fadd.vv.f32f32f32.lat.L1
    restore_caller_vec
    ret
###############################
// fmul: Pattern A' (vd fed back into vn).
#void asimd_fmul_vv_f32f32f32_latency(int64_t);
    PROLOGUE asimd_fmul_vv_f32f32f32_latency
    preserve_caller_vec
    INIT
    // v0 must be non-zero or fmul chain stays at zero forever; INIT zeroes it.
    // Set v0 = 1.0 in all lanes so fmul has stable, non-degenerate input.
    fmov v0.4s, #1.0
    fmov v8.4s, #1.0
.asimd.fmul.vv.f32f32f32.lat.L1:
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    subs x0, x0, #1
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    fmul v8.4s, v8.4s, v0.4s
    bne .asimd.fmul.vv.f32f32f32.lat.L1
    restore_caller_vec
    ret
###############################
