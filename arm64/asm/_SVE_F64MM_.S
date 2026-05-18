.align 5
#include "common_arm64.h"

// FEAT_F64MM — fmmla z.d, z.d, z.d
// Per-instruction at VL=128b: 2x2 outer-product with 2-element fp64 dot
//   -> 2*2*2*2 = 8 FLOPs at VL=128b. Register 12 per loop ( = 24 * 8 / 16 ).

.macro SVE_F64MM_INIT
    fmov z0.d, p0/m, #0
    fmov z1.d, p0/m, #0
    fmov z8.d, p0/m, #0
    fmov z9.d, p0/m, #0
    fmov z10.d, p0/m, #0
    fmov z11.d, p0/m, #0
    fmov z12.d, p0/m, #0
    fmov z13.d, p0/m, #0
    fmov z14.d, p0/m, #0
    fmov z15.d, p0/m, #0
    fmov z16.d, p0/m, #0
    fmov z17.d, p0/m, #0
    fmov z18.d, p0/m, #0
    fmov z19.d, p0/m, #0
    fmov z20.d, p0/m, #0
    fmov z21.d, p0/m, #0
    fmov z22.d, p0/m, #0
    fmov z23.d, p0/m, #0
    fmov z24.d, p0/m, #0
    fmov z25.d, p0/m, #0
    fmov z26.d, p0/m, #0
    fmov z27.d, p0/m, #0
    fmov z28.d, p0/m, #0
    fmov z29.d, p0/m, #0
    fmov z30.d, p0/m, #0
    fmov z31.d, p0/m, #0
.endm

#void sve_fmmla_f64f64f64(int64_t);
#void sve_fmmla_f64f64f64_latency(int64_t);

############
    PROLOGUE sve_fmmla_f64f64f64
    preserve_caller_vec
    ptrue p0.d
    SVE_F64MM_INIT
.sve_fmmla_f64f64f64L1:
    fmmla z8.d,  z0.d, z1.d
    fmmla z9.d,  z0.d, z1.d
    fmmla z10.d, z0.d, z1.d
    fmmla z11.d, z0.d, z1.d
    fmmla z12.d, z0.d, z1.d
    fmmla z13.d, z0.d, z1.d
    fmmla z14.d, z0.d, z1.d
    fmmla z15.d, z0.d, z1.d
    fmmla z16.d, z0.d, z1.d
    fmmla z17.d, z0.d, z1.d
    fmmla z18.d, z0.d, z1.d
    fmmla z19.d, z0.d, z1.d
    subs x0, x0, #1
    fmmla z20.d, z0.d, z1.d
    fmmla z21.d, z0.d, z1.d
    fmmla z22.d, z0.d, z1.d
    fmmla z23.d, z0.d, z1.d
    fmmla z24.d, z0.d, z1.d
    fmmla z25.d, z0.d, z1.d
    fmmla z26.d, z0.d, z1.d
    fmmla z27.d, z0.d, z1.d
    fmmla z28.d, z0.d, z1.d
    fmmla z29.d, z0.d, z1.d
    fmmla z30.d, z0.d, z1.d
    fmmla z31.d, z0.d, z1.d
    bne .sve_fmmla_f64f64f64L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_fmmla_f64f64f64_latency
    preserve_caller_vec
    ptrue p0.d
    SVE_F64MM_INIT
.sve_fmmla_f64f64f64_latencyL1:
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    subs x0, x0, #1
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    fmmla z8.d, z0.d, z1.d
    bne .sve_fmmla_f64f64f64_latencyL1
    restore_caller_vec
    ret
