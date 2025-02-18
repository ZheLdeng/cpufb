.align 5
#include "common_arm64.h"
.macro SME_INIT
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

    ptrue p0.s
    ptrue p1.s
    mov w8, #0
    mov w9, #1
    mov w10, #2
    mov w11, #3
.endm

############
    PROLOGUE sme2_bfmlal_vs_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT
.sme2_bfmlal_vs_f32bf16bf16L1:
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    subs x0, x0, #1
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    bne .sme2_bfmlal_vs_f32bf16bf16L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme2_bfmlal4_vs_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT
.sme2_bfmlal4_vs_f32bf16bf16L1:
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    subs x0, x0, #1
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    bne .sme2_bfmlal4_vs_f32bf16bf16L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme2_bfmlal_vv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT
.sme2_bfmlal_vv_f32bf16bf16L1:
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    subs x0, x0, #1
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    BFMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    bne .sme2_bfmlal_vv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret

####
    PROLOGUE sme2_bfmlal4_vv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT
.sme2_bfmlal4_vv_f32bf16bf16L1:
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    subs x0, x0, #1
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    bne .sme2_bfmlal4_vv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfmlal_mvv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfmlal_mvv_f32bf16bf16L1:
    BFMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    BFMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_bfmlal_mvv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfmlal4_mvv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfmlal4_mvv_f32bf16bf16L1:
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_bfmlal4_mvv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfdot_vs_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfdot_vs_f32bf16bf16L1:
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H[0]
    subs x0, x0, #1
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H[0]
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H[0]
    bne .sme2_bfdot_vs_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfdot4_vs_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfdot4_vs_f32bf16bf16L1:
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    subs x0, x0, #1
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    bne .sme2_bfdot4_vs_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfdot_vv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfdot_vv_f32bf16bf16L1:
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    subs x0, x0, #1
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    bne .sme2_bfdot_vv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfdot4_vv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfdot4_vv_f32bf16bf16L1:
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    subs x0, x0, #1
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    bne .sme2_bfdot4_vv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfdot_mvv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfdot_mvv_f32bf16bf16L1:
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_bfdot_mvv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_bfdot4_mvv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_bfdot4_mvv_f32bf16bf16L1:
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    BFDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_bfdot4_mvv_f32bf16bf16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla_vs_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla_vs_f32bf32f32L1:
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S[0]
    subs x0, x0, #1
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S[0]
    bne .sme2_fmla_vs_f32bf32f32L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla4_vs_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla4_vs_f32f32f32L1:
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    subs x0, x0, #1
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S[0]
    bne .sme2_fmla4_vs_f32f32f32L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla_vv_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla_vv_f32f32f32L1:
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S
    subs x0, x0, #1
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0], { z0.S-z3.S }, z4.S
    bne .sme2_fmla_vv_f32f32f32L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla4_vv_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla4_vv_f32f32f32L1:
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S
    subs x0, x0, #1
    FMLA ZA.S[w8, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 0, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w8, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w9, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w10, 4, VGx4], { z0.S-z3.S }, z4.S
    FMLA ZA.S[w11, 4, VGx4], { z0.S-z3.S }, z4.S
    bne .sme2_fmla4_vv_f32f32f32L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla_mvv_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla_mvv_f32f32f32L1:
    FMLA ZA.S[w8, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 0], {z0.s-z3.s}, {z0.s-z3.s}
    subs x0, x0, #1
    FMLA ZA.S[w8, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 0], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 4], {z0.s-z3.s}, {z0.s-z3.s}
    bne .sme2_fmla_mvv_f32f32f32L1
    smstop
    restore_caller_vec
    ret

####
    PROLOGUE sme2_fmla4_mvv_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla4_mvv_f32f32f32L1:
    FMLA ZA.S[w8, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    subs x0, x0, #1
    FMLA ZA.S[w8, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 0, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w8, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w9, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w10, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    FMLA ZA.S[w11, 4, VGx4], {z0.s-z3.s}, {z0.s-z3.s}
    bne .sme2_fmla4_mvv_f32f32f32L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmlal_vs_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmlal_vs_f32f16f16L1:
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    subs x0, x0, #1
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H[0]
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H[0]
    bne .sme2_fmlal_vs_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmlal4_vs_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmlal4_vs_f32f16f16L1:
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    subs x0, x0, #1
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h[0]
    bne .sme2_fmlal4_vs_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmlal_vv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmlal_vv_f32f16f16L1:
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    subs x0, x0, #1
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w8, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w9, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w10, 0:1], Z4.H, Z5.H
    FMLAL ZA.S[w11, 0:1], Z4.H, Z5.H
    bne .sme2_fmlal_vv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmlal4_vv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmlal4_vv_f32f16f16L1:
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    subs x0, x0, #1
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, z5.h
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, z5.h
    bne .sme2_fmlal4_vv_f32f16f16L1
    smstop
    restore_caller_vec
    ret


####
    PROLOGUE sme2_fmlal_mvv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmlal_mvv_f32f16f16L1:
    FMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    FMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_fmlal_mvv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmlal4_mvv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmlal4_mvv_f32f16f16L1:
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w8, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w9, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w10, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FMLAL ZA.S[w11, 0:1, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_fmlal4_mvv_f32f16f16L1
    smstop
    restore_caller_vec
    ret

####
    PROLOGUE sme2_fvdot_vs_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fvdot_vs_f32f16f16L1:
    FVDOT ZA.S[w8, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0], {z0.h-z1.h}, z5.h[0]
    subs x0, x0, #1
    FVDOT ZA.S[w8, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0], {z0.h-z1.h}, z5.h[0]
    bne .sme2_fvdot_vs_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fvdot2_vs_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fvdot2_vs_f32f16f16L1:
    FVDOT ZA.S[w8, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    subs x0, x0, #1
    FVDOT ZA.S[w8, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 0, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w8, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w9, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w10, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    FVDOT ZA.S[w11, 4, VGx2], {z0.h-z1.h}, z5.h[0]
    bne .sme2_fvdot2_vs_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fdot_vs_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fdot_vs_f32f16f16L1:
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, z5.h[0]
    subs x0, x0, #1
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, z5.h[0]
    bne .sme2_fdot_vs_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fdot4_vs_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fdot4_vs_f32f16f16L1:
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    subs x0, x0, #1
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h[0]
    bne .sme2_fdot4_vs_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fdot_vv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fdot_vv_f32f16f16L1:
    FDOT ZA.S[W8, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    subs x0, x0, #1
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, Z5.H
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, Z5.H
    bne .sme2_fdot_vv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fdot4_vv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fdot4_vv_f32f16f16L1:
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    subs x0, x0, #1
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, z5.h
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, z5.h
    bne .sme2_fdot4_vv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fdot_mvv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fdot_mvv_f32f16f16L1:
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_fdot_mvv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fdot4_mvv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fdot4_mvv_f32f16f16L1:
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    subs x0, x0, #1
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w8, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w9, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w10, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    FDOT ZA.S[w11, 0, VGx4], {z0.h-z3.h}, {z0.h-z3.h}
    bne .sme2_fdot4_mvv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
