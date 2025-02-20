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
.endm

############
    PROLOGUE sme_fmopa_vv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT
.sme_fmopa_vv_f64f64f64L1:
    FMOPA za0.d, p0/M, p1/M, z8.d, z8.d
    FMOPA za1.d, p0/M, p1/M, z9.d, z9.d
    FMOPA za2.d, p0/M, p1/M, z10.d, z10.d
    FMOPA za3.d, p0/M, p1/M, z11.d, z11.d
    FMOPA za4.d, p0/M, p1/M, z12.d, z12.d
    FMOPA za5.d, p0/M, p1/M, z13.d, z13.d
    FMOPA za6.d, p0/M, p1/M, z14.d, z14.d
    FMOPA za7.d, p0/M, p1/M, z15.d, z15.d
    FMOPA za0.d, p0/M, p1/M, z16.d, z16.d
    FMOPA za1.d, p0/M, p1/M, z17.d, z17.d
    FMOPA za2.d, p0/M, p1/M, z18.d, z18.d
    FMOPA za3.d, p0/M, p1/M, z19.d, z19.d
    subs x0, x0, #1
    FMOPA za4.d, p0/M, p1/M, z20.d, z20.d
    FMOPA za5.d, p0/M, p1/M, z21.d, z21.d
    FMOPA za6.d, p0/M, p1/M, z22.d, z22.d
    FMOPA za7.d, p0/M, p1/M, z23.d, z23.d
    FMOPA za0.d, p0/M, p1/M, z24.d, z24.d
    FMOPA za1.d, p0/M, p1/M, z25.d, z25.d
    FMOPA za2.d, p0/M, p1/M, z26.d, z26.d
    FMOPA za3.d, p0/M, p1/M, z27.d, z27.d
    FMOPA za4.d, p0/M, p1/M, z28.d, z28.d
    FMOPA za5.d, p0/M, p1/M, z29.d, z29.d
    FMOPA za6.d, p0/M, p1/M, z30.d, z30.d
    FMOPA za7.d, p0/M, p1/M, z31.d, z31.d
    bne .sme_fmopa_vv_f64f64f64L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme_fmopa2_vv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT
.sme_fmopa2_vv_f64f64f64L1:
    FMOPA za0.d, p0/M, p1/M, z8.d, z8.d
    FMOPA za0.d, p0/M, p1/M, z9.d, z9.d
    FMOPA za0.d, p0/M, p1/M, z10.d, z10.d
    FMOPA za0.d, p0/M, p1/M, z11.d, z11.d
    FMOPA za0.d, p0/M, p1/M, z12.d, z12.d
    FMOPA za0.d, p0/M, p1/M, z13.d, z13.d
    FMOPA za0.d, p0/M, p1/M, z14.d, z14.d
    FMOPA za0.d, p0/M, p1/M, z15.d, z15.d
    FMOPA za0.d, p0/M, p1/M, z16.d, z16.d
    FMOPA za0.d, p0/M, p1/M, z17.d, z17.d
    FMOPA za0.d, p0/M, p1/M, z18.d, z18.d
    FMOPA za0.d, p0/M, p1/M, z19.d, z19.d
    subs x0, x0, #1
    FMOPA za0.d, p0/M, p1/M, z20.d, z20.d
    FMOPA za0.d, p0/M, p1/M, z21.d, z21.d
    FMOPA za0.d, p0/M, p1/M, z22.d, z22.d
    FMOPA za0.d, p0/M, p1/M, z23.d, z23.d
    FMOPA za0.d, p0/M, p1/M, z24.d, z24.d
    FMOPA za0.d, p0/M, p1/M, z25.d, z25.d
    FMOPA za0.d, p0/M, p1/M, z26.d, z26.d
    FMOPA za0.d, p0/M, p1/M, z27.d, z27.d
    FMOPA za0.d, p0/M, p1/M, z28.d, z28.d
    FMOPA za0.d, p0/M, p1/M, z29.d, z29.d
    FMOPA za0.d, p0/M, p1/M, z30.d, z30.d
    FMOPA za0.d, p0/M, p1/M, z31.d, z31.d
    bne .sme_fmopa2_vv_f64f64f64L1
    restore_caller_vec
    smstop
    ret

####
    PROLOGUE sme2_fmla_vs_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla_vs_f64f64f64L1:
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 0], { z0.D-z3.D }, z4.D[0]
    subs x0, x0, #1
    FMLA ZA.D[w8, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 4], { z0.D-z3.D }, z4.D[0]
    bne .sme2_fmla_vs_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla2_vs_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla2_vs_f64f64f64L1:
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    subs x0, x0, #1
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D[0]
    bne .sme2_fmla2_vs_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla4_vs_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla4_vs_f64f64f64L1:
    FMLA ZA.D[w8, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    subs x0, x0, #1
    FMLA ZA.D[w8, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 0, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w8, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w9, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w10, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    FMLA ZA.D[w11, 4, VGx4], { z0.D-z3.D }, z4.D[0]
    bne .sme2_fmla4_vs_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla_vv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla_vv_f64f64f64L1:
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 0], { z0.D-z3.D }, z4.D
    subs x0, x0, #1
    FMLA ZA.D[w8, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 4], { z0.D-z3.D }, z4.D
    bne .sme2_fmla_vv_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla2_vv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla2_vv_f64f64f64L1:
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    subs x0, x0, #1
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0], { z0.D-z3.D }, z4.D
    bne .sme2_fmla2_vv_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla4_vv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla4_vv_f64f64f64L1:
    FMLA ZA.D[w8, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 0, VGx4], { z0.D-z3.D }, z4.D
    subs x0, x0, #1
    FMLA ZA.D[w8, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 0, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w8, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w9, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w10, 4, VGx4], { z0.D-z3.D }, z4.D
    FMLA ZA.D[w11, 4, VGx4], { z0.D-z3.D }, z4.D
    bne .sme2_fmla4_vv_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla_mvv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla_mvv_f64f64f64L1:
    FMLA ZA.D[w8, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 0], {z0.d-z3.d}, {z0.d-z3.d}
    subs x0, x0, #1
    FMLA ZA.D[w8, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 0], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 4], {z0.d-z3.d}, {z0.d-z3.d}
    bne .sme2_fmla_mvv_f64f64f64L1
    smstop
    restore_caller_vec
    ret
####
    PROLOGUE sme2_fmla4_mvv_f64f64f64
    preserve_caller_vec
    smstart
    SME_INIT

.sme2_fmla4_mvv_f64f64f64L1:
    FMLA ZA.D[w8, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    subs x0, x0, #1
    FMLA ZA.D[w8, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 0, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w8, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w9, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w10, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    FMLA ZA.D[w11, 4, VGx4], {z0.d-z3.d}, {z0.d-z3.d}
    bne .sme2_fmla4_mvv_f64f64f64L1
    smstop
    restore_caller_vec
    ret