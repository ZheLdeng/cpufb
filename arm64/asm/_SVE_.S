.align 5
#include "common_arm64.h"
.macro SVE_INIT
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

############

    PROLOGUE sve_fmla_vs_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vs_f32f32f32L1:
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
    bne .sve_fmla_vs_f32f32f32L1
    restore_caller_vec
    ret

############

    PROLOGUE sve_fmla_vv_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vv_f32f32f32L1:
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z9.s, p0/m, z9.s, z9.s
    fmla z10.s, p0/m, z10.s, z10.s
    fmla z11.s, p0/m, z11.s, z11.s
    fmla z12.s, p0/m, z12.s, z12.s
    fmla z13.s, p0/m, z13.s, z13.s
    fmla z14.s, p0/m, z14.s, z14.s
    fmla z15.s, p0/m, z15.s, z15.s
    fmla z16.s, p0/m, z16.s, z16.s
    fmla z17.s, p0/m, z17.s, z17.s
    fmla z18.s, p0/m, z18.s, z18.s
    fmla z19.s, p0/m, z19.s, z19.s
    subs x0, x0, #1
    fmla z20.s, p0/m, z20.s, z20.s
    fmla z21.s, p0/m, z21.s, z21.s
    fmla z22.s, p0/m, z22.s, z22.s
    fmla z23.s, p0/m, z23.s, z23.s
    fmla z24.s, p0/m, z24.s, z24.s
    fmla z25.s, p0/m, z25.s, z25.s
    fmla z26.s, p0/m, z26.s, z26.s
    fmla z27.s, p0/m, z27.s, z27.s
    fmla z28.s, p0/m, z28.s, z28.s
    fmla z29.s, p0/m, z29.s, z29.s
    fmla z30.s, p0/m, z30.s, z30.s
    fmla z31.s, p0/m, z31.s, z31.s
    bne .sve_fmla_vv_f32f32f32L1
    restore_caller_vec
    ret

############

    PROLOGUE sve_fmla_vs_f64f64f64
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vs_f64f64f64L1:
    fmla z8.d, z0.d, z1.d[0]
    fmla z9.d, z0.d, z1.d[0]
    fmla z10.d, z0.d, z1.d[0]
    fmla z11.d, z0.d, z1.d[0]
    fmla z12.d, z0.d, z1.d[0]
    fmla z13.d, z0.d, z1.d[0]
    fmla z14.d, z0.d, z1.d[0]
    fmla z15.d, z0.d, z1.d[0]
    fmla z16.d, z0.d, z1.d[0]
    fmla z17.d, z0.d, z1.d[0]
    fmla z18.d, z0.d, z1.d[0]
    fmla z19.d, z0.d, z1.d[0]
    subs x0, x0, #1
    fmla z20.d, z0.d, z1.d[0]
    fmla z21.d, z0.d, z1.d[0]
    fmla z22.d, z0.d, z1.d[0]
    fmla z23.d, z0.d, z1.d[0]
    fmla z24.d, z0.d, z1.d[0]
    fmla z25.d, z0.d, z1.d[0]
    fmla z26.d, z0.d, z1.d[0]
    fmla z27.d, z0.d, z1.d[0]
    fmla z28.d, z0.d, z1.d[0]
    fmla z29.d, z0.d, z1.d[0]
    fmla z30.d, z0.d, z1.d[0]
    fmla z31.d, z0.d, z1.d[0]
    bne .sve_fmla_vs_f64f64f64L1
    restore_caller_vec
    ret

############

    PROLOGUE sve_fmla_vv_f64f64f64
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vv_f64f64f64L1:
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z9.d, p0/m, z9.d, z9.d
    fmla z10.d, p0/m, z10.d, z10.d
    fmla z11.d, p0/m, z11.d, z11.d
    fmla z12.d, p0/m, z12.d, z12.d
    fmla z13.d, p0/m, z13.d, z13.d
    fmla z14.d, p0/m, z14.d, z14.d
    fmla z15.d, p0/m, z15.d, z15.d
    fmla z16.d, p0/m, z16.d, z16.d
    fmla z17.d, p0/m, z17.d, z17.d
    fmla z18.d, p0/m, z18.d, z18.d
    fmla z19.d, p0/m, z19.d, z19.d
    subs x0, x0, #1
    fmla z20.d, p0/m, z20.d, z20.d
    fmla z21.d, p0/m, z21.d, z21.d
    fmla z22.d, p0/m, z22.d, z22.d
    fmla z23.d, p0/m, z23.d, z23.d
    fmla z24.d, p0/m, z24.d, z24.d
    fmla z25.d, p0/m, z25.d, z25.d
    fmla z26.d, p0/m, z26.d, z26.d
    fmla z27.d, p0/m, z27.d, z27.d
    fmla z28.d, p0/m, z28.d, z28.d
    fmla z29.d, p0/m, z29.d, z29.d
    fmla z30.d, p0/m, z30.d, z30.d
    fmla z31.d, p0/m, z31.d, z31.d
    bne .sve_fmla_vv_f64f64f64L1
    restore_caller_vec
    ret

#void void load_ld1w_kernel(float*, int ,int);
    PROLOGUE load_ld1w_kernel
    mov x11, x2
    ptrue p2.s
outer_loop:
    mov x9, x0
    mov x10, x1
    whilelt p1.b, xzr, x10
inner_loop:
    ld1w z0.s, p2/z, [x9]
    ld1w z1.s, p2/z, [x9, #1, MUL VL]
    ld1w z2.s, p2/z, [x9, #2, MUL VL]
    ld1w z3.s, p2/z, [x9, #3, MUL VL]
    ld1w z4.s, p2/z, [x9, #4, MUL VL]
    ld1w z5.s, p2/z, [x9, #5, MUL VL]
    ld1w z6.s, p2/z, [x9, #6, MUL VL]
    ld1w z7.s, p2/z, [x9, #7, MUL VL]
    incb  x9, POW2, MUL#8
    decb x10, pow2, mul#2
    whilelt p1.s, xzr, x10
    b.first inner_loop

    subs x11, x11, #1
    bne outer_loop
out:
    ret
