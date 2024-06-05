.align 5
#include "common_arm64.h"
.macro SVE_INIT
    fmov z0.b, p0/m, #0
    fmov z1.b, p0/m, #0
    fmov z8.b, p0/m, #0
    fmov z9.b, p0/m, #0
    fmov z10.b, p0/m, #0
    fmov z11.b, p0/m, #0
    fmov z12.b, p0/m, #0
    fmov z13.b, p0/m, #0
    fmov z14.b, p0/m, #0
    fmov z15.b, p0/m, #0
    fmov z16.b, p0/m, #0
    fmov z17.b, p0/m, #0
    fmov z18.b, p0/m, #0
    fmov z19.b, p0/m, #0
    fmov z20.b, p0/m, #0
    fmov z21.b, p0/m, #0
    fmov z22.b, p0/m, #0
    fmov z23.b, p0/m, #0
    fmov z24.b, p0/m, #0
    fmov z25.b, p0/m, #0
    fmov z26.b, p0/m, #0
    fmov z27.b, p0/m, #0
    fmov z28.b, p0/m, #0
    fmov z29.b, p0/m, #0
    fmov z30.b, p0/m, #0
    fmov z31.b, p0/m, #0
.endm

############

    PROLOGUE sve_fmla_vs_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.L1:
    fmla z8.s, z0.s, z1.s[0]
    fmla z9.s, z0.s, z1.s[0]
    fmla z10.s, z0.s, z1.s[0]
    fmla z11.s, z0.s, z1.s[0]
    fmla z12.s, z0.s, z1.s[0]
    fmla z13.s, z0.s, z1.s[0]
    fmla z14.s, z0.s, z1.s[0]
    fmla z15.s, z0.s, z1.s[0]
    fmla z16.s, z0.s, z1.s[0]
    fmla z17.s, z0.s, z1.s[0]
    fmla z18.s, z0.s, z1.s[0]
    fmla z19.s, z0.s, z1.s[0]
    subs x0, x0, #1
    fmla z20.s, z0.s, z1.s[0]
    fmla z21.s, z0.s, z1.s[0]
    fmla z22.s, z0.s, z1.s[0]
    fmla z23.s, z0.s, z1.s[0]
    fmla z24.s, z0.s, z1.s[0]
    fmla z25.s, z0.s, z1.s[0]
    fmla z26.s, z0.s, z1.s[0]
    fmla z27.s, z0.s, z1.s[0]
    fmla z28.s, z0.s, z1.s[0]
    fmla z29.s, z0.s, z1.s[0]
    fmla z30.s, z0.s, z1.s[0]
    fmla z31.s, z0.s, z1.s[0]
    bne .L1
    restore_caller_vec
    ret
