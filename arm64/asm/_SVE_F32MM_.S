.align 5
#include "common_arm64.h"

// FEAT_F32MM — fmmla z.s, z.s, z.s
// Per-instruction at VL=128b: 2x4 (2 rows of 4 fp32 outputs) outer-product
//   with 4-element fp32 dot per cell -> 2*4*4*2 = 32 FLOPs at VL=128b.
// Runtime FLOPS scaled by svcntb()/16 at registration; we register 48 per
// loop ( = 24 inst * 32 FLOPs / 16 ), then cpufb.cpp multiplies by svcntb().
//
// FEAT_F64MM — fmmla z.d, z.d, z.d
// Per-instruction at VL=128b: 2x2 outer-product with 2-element fp64 dot
//   -> 2*2*2*2 = 8 FLOPs at VL=128b. Register 12 per loop ( = 24 * 8 / 16 ).

.macro SVE_MM_INIT
    fmov z0.s, p0/m, #0
    fmov z1.s, p0/m, #0
    fmov z8.s, p0/m, #0
    fmov z9.s, p0/m, #0
    fmov z10.s, p0/m, #0
    fmov z11.s, p0/m, #0
    fmov z12.s, p0/m, #0
    fmov z13.s, p0/m, #0
    fmov z14.s, p0/m, #0
    fmov z15.s, p0/m, #0
    fmov z16.s, p0/m, #0
    fmov z17.s, p0/m, #0
    fmov z18.s, p0/m, #0
    fmov z19.s, p0/m, #0
    fmov z20.s, p0/m, #0
    fmov z21.s, p0/m, #0
    fmov z22.s, p0/m, #0
    fmov z23.s, p0/m, #0
    fmov z24.s, p0/m, #0
    fmov z25.s, p0/m, #0
    fmov z26.s, p0/m, #0
    fmov z27.s, p0/m, #0
    fmov z28.s, p0/m, #0
    fmov z29.s, p0/m, #0
    fmov z30.s, p0/m, #0
    fmov z31.s, p0/m, #0
.endm

#void sve_fmmla_f32f32f32(int64_t);
#void sve_fmmla_f32f32f32_latency(int64_t);

############
    PROLOGUE sve_fmmla_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_MM_INIT
.sve_fmmla_f32f32f32L1:
    fmmla z8.s,  z0.s, z1.s
    fmmla z9.s,  z0.s, z1.s
    fmmla z10.s, z0.s, z1.s
    fmmla z11.s, z0.s, z1.s
    fmmla z12.s, z0.s, z1.s
    fmmla z13.s, z0.s, z1.s
    fmmla z14.s, z0.s, z1.s
    fmmla z15.s, z0.s, z1.s
    fmmla z16.s, z0.s, z1.s
    fmmla z17.s, z0.s, z1.s
    fmmla z18.s, z0.s, z1.s
    fmmla z19.s, z0.s, z1.s
    subs x0, x0, #1
    fmmla z20.s, z0.s, z1.s
    fmmla z21.s, z0.s, z1.s
    fmmla z22.s, z0.s, z1.s
    fmmla z23.s, z0.s, z1.s
    fmmla z24.s, z0.s, z1.s
    fmmla z25.s, z0.s, z1.s
    fmmla z26.s, z0.s, z1.s
    fmmla z27.s, z0.s, z1.s
    fmmla z28.s, z0.s, z1.s
    fmmla z29.s, z0.s, z1.s
    fmmla z30.s, z0.s, z1.s
    fmmla z31.s, z0.s, z1.s
    bne .sve_fmmla_f32f32f32L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_fmmla_f32f32f32_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_MM_INIT
.sve_fmmla_f32f32f32_latencyL1:
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    subs x0, x0, #1
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    fmmla z8.s, z0.s, z1.s
    bne .sve_fmmla_f32f32f32_latencyL1
    restore_caller_vec
    ret
