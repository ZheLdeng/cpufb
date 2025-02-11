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
    PROLOGUE sme_bfmopa_vv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT
.sme_bfmopa_vv_f32bf16bf16L1:
    BFMOPA za0.S, p0/M, p1/M, z8.H, z8.H
    BFMOPA za1.S, p0/M, p1/M, z9.H, z9.H
    BFMOPA za2.S, p0/M, p1/M, z10.H, z10.H
    BFMOPA za3.S, p0/M, p1/M, z11.H, z11.H
    BFMOPA za0.S, p0/M, p1/M, z12.H, z12.H
    BFMOPA za1.S, p0/M, p1/M, z13.H, z13.H
    BFMOPA za2.S, p0/M, p1/M, z14.H, z14.H
    BFMOPA za3.S, p0/M, p1/M, z15.H, z15.H
    BFMOPA za0.S, p0/M, p1/M, z16.H, z16.H
    BFMOPA za1.S, p0/M, p1/M, z17.H, z17.H
    BFMOPA za2.S, p0/M, p1/M, z18.H, z18.H
    BFMOPA za3.S, p0/M, p1/M, z19.H, z19.H
    subs x0, x0, #1
    BFMOPA za0.S, p0/M, p1/M, z20.H, z20.H
    BFMOPA za1.S, p0/M, p1/M, z21.H, z21.H
    BFMOPA za2.S, p0/M, p1/M, z22.H, z22.H
    BFMOPA za3.S, p0/M, p1/M, z23.H, z23.H
    BFMOPA za0.S, p0/M, p1/M, z24.H, z24.H
    BFMOPA za1.S, p0/M, p1/M, z25.H, z25.H
    BFMOPA za2.S, p0/M, p1/M, z26.H, z26.H
    BFMOPA za3.S, p0/M, p1/M, z27.H, z27.H
    BFMOPA za0.S, p0/M, p1/M, z28.H, z28.H
    BFMOPA za1.S, p0/M, p1/M, z29.H, z29.H
    BFMOPA za2.S, p0/M, p1/M, z30.H, z30.H
    BFMOPA za3.S, p0/M, p1/M, z31.H, z31.H
    bne .sme_bfmopa_vv_f32bf16bf16L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme_fmopa_vv_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT
.sme_fmopa_vv_f32f32f32L1:
    FMOPA za0.S, p0/M, p1/M, z8.S, z9.S
    FMOPA za1.S, p0/M, p1/M, z10.S, z11.S
    FMOPA za2.S, p0/M, p1/M, z12.S, z13.S
    FMOPA za3.S, p0/M, p1/M, z14.S, z15.S
    FMOPA za0.S, p0/M, p1/M, z16.S, z17.S
    FMOPA za1.S, p0/M, p1/M, z18.S, z19.S
    FMOPA za2.S, p0/M, p1/M, z20.S, z21.S
    FMOPA za3.S, p0/M, p1/M, z22.S, z23.S
    FMOPA za0.S, p0/M, p1/M, z24.S, z25.S
    FMOPA za1.S, p0/M, p1/M, z26.S, z27.S
    FMOPA za2.S, p0/M, p1/M, z28.S, z29.S
    FMOPA za3.S, p0/M, p1/M, z30.S, z31.S
    subs x0, x0, #1
    FMOPA za0.S, p0/M, p1/M, z8.S, z9.S
    FMOPA za1.S, p0/M, p1/M, z10.S, z11.S
    FMOPA za2.S, p0/M, p1/M, z12.S, z13.S
    FMOPA za3.S, p0/M, p1/M, z14.S, z15.S
    FMOPA za0.S, p0/M, p1/M, z16.S, z17.S
    FMOPA za1.S, p0/M, p1/M, z18.S, z19.S
    FMOPA za2.S, p0/M, p1/M, z20.S, z21.S
    FMOPA za3.S, p0/M, p1/M, z22.S, z23.S
    FMOPA za0.S, p0/M, p1/M, z24.S, z25.S
    FMOPA za1.S, p0/M, p1/M, z26.S, z27.S
    FMOPA za2.S, p0/M, p1/M, z28.S, z29.S
    FMOPA za3.S, p0/M, p1/M, z30.S, z31.S
    bne .sme_fmopa_vv_f32f32f32L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme_fmopa_vv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT
.sme_fmopa_vv_f32f16f16L1:
    FMOPA za0.S, p0/M, p1/M, z8.H, z9.H
    FMOPA za1.S, p0/M, p1/M, z10.H, z11.H
    FMOPA za2.S, p0/M, p1/M, z12.H, z13.H
    FMOPA za3.S, p0/M, p1/M, z14.H, z15.H
    FMOPA za0.S, p0/M, p1/M, z16.H, z17.H
    FMOPA za1.S, p0/M, p1/M, z18.H, z19.H
    FMOPA za2.S, p0/M, p1/M, z20.H, z21.H
    FMOPA za3.S, p0/M, p1/M, z22.H, z23.H
    FMOPA za0.S, p0/M, p1/M, z24.H, z25.H
    FMOPA za1.S, p0/M, p1/M, z26.H, z27.H
    FMOPA za2.S, p0/M, p1/M, z28.H, z29.H
    FMOPA za3.S, p0/M, p1/M, z30.H, z31.H
    subs x0, x0, #1
    FMOPA za0.S, p0/M, p1/M, z8.H, z9.H
    FMOPA za1.S, p0/M, p1/M, z10.H, z11.H
    FMOPA za2.S, p0/M, p1/M, z12.H, z13.H
    FMOPA za3.S, p0/M, p1/M, z14.H, z15.H
    FMOPA za0.S, p0/M, p1/M, z16.H, z17.H
    FMOPA za1.S, p0/M, p1/M, z18.H, z19.H
    FMOPA za2.S, p0/M, p1/M, z20.H, z21.H
    FMOPA za3.S, p0/M, p1/M, z22.H, z23.H
    FMOPA za0.S, p0/M, p1/M, z24.H, z25.H
    FMOPA za1.S, p0/M, p1/M, z26.H, z27.H
    FMOPA za2.S, p0/M, p1/M, z28.H, z29.H
    FMOPA za3.S, p0/M, p1/M, z30.H, z31.H
    bne .sme_fmopa_vv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
############
    PROLOGUE sme_smopa_vv_i32i8i8
    preserve_caller_vec
    smstart
    SME_INIT
.sme_smopa_vv_i32i8i8L1:
    SMOPA za0.S, p0/M, p1/M, z8.B, z9.B
    SMOPA za1.S, p0/M, p1/M, z10.B, z11.B
    SMOPA za2.S, p0/M, p1/M, z12.B, z13.B
    SMOPA za3.S, p0/M, p1/M, z14.B, z15.B
    SMOPA za0.S, p0/M, p1/M, z16.B, z17.B
    SMOPA za1.S, p0/M, p1/M, z18.B, z19.B
    SMOPA za2.S, p0/M, p1/M, z20.B, z21.B
    SMOPA za3.S, p0/M, p1/M, z22.B, z23.B
    SMOPA za0.S, p0/M, p1/M, z24.B, z25.B
    SMOPA za1.S, p0/M, p1/M, z26.B, z27.B
    SMOPA za2.S, p0/M, p1/M, z28.B, z29.B
    SMOPA za3.S, p0/M, p1/M, z30.B, z31.B
    subs x0, x0, #1
    SMOPA za0.S, p0/M, p1/M, z8.B, z9.B
    SMOPA za1.S, p0/M, p1/M, z10.B, z11.B
    SMOPA za2.S, p0/M, p1/M, z12.B, z13.B
    SMOPA za3.S, p0/M, p1/M, z14.B, z15.B
    SMOPA za0.S, p0/M, p1/M, z16.B, z17.B
    SMOPA za1.S, p0/M, p1/M, z18.B, z19.B
    SMOPA za2.S, p0/M, p1/M, z20.B, z21.B
    SMOPA za3.S, p0/M, p1/M, z22.B, z23.B
    SMOPA za0.S, p0/M, p1/M, z24.B, z25.B
    SMOPA za1.S, p0/M, p1/M, z26.B, z27.B
    SMOPA za2.S, p0/M, p1/M, z28.B, z29.B
    SMOPA za3.S, p0/M, p1/M, z30.B, z31.B
    bne .sme_smopa_vv_i32i8i8L1
    restore_caller_vec
    smstop
    ret
############