.align 5
#include "common_arm64.h"

// FEAT_SME_I16I32 (commonly enabled by sme-i16i64 march flag) — 16-bit
// integer outer-products with 32-bit accumulators.
//   smopa za.s, p/m, p/m, z.h, z.h
//   umopa za.s, p/m, p/m, z.h, z.h
// Per-instruction at SVL=N bytes: (N/4) x (N/4) outputs * 2 (16-bit
// elements per dot) * 2 ops = (rdsvl()^2)/4. Register comp_pl = 24, then
// cpufb.cpp scales by rdsvl()*rdsvl()/4/2 in the SME comp_pl block.

.macro SME_I16_INIT
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
    ptrue p0.h
    ptrue p1.h
.endm

#void sme_smopa_vv_i32i16i16(int64_t);
#void sme_umopa_vv_i32i16i16(int64_t);
#void sme_smopa2_vv_i32i16i16(int64_t);
#void sme_umopa2_vv_i32i16i16(int64_t);

############
    PROLOGUE sme_smopa_vv_i32i16i16
    preserve_caller_vec
    smstart
    SME_I16_INIT
.sme_smopa_vv_i32i16i16L1:
    smopa za0.s, p0/m, p1/m, z8.h,  z9.h
    smopa za1.s, p0/m, p1/m, z10.h, z11.h
    smopa za2.s, p0/m, p1/m, z12.h, z13.h
    smopa za3.s, p0/m, p1/m, z14.h, z15.h
    smopa za0.s, p0/m, p1/m, z16.h, z17.h
    smopa za1.s, p0/m, p1/m, z18.h, z19.h
    smopa za2.s, p0/m, p1/m, z20.h, z21.h
    smopa za3.s, p0/m, p1/m, z22.h, z23.h
    smopa za0.s, p0/m, p1/m, z24.h, z25.h
    smopa za1.s, p0/m, p1/m, z26.h, z27.h
    smopa za2.s, p0/m, p1/m, z28.h, z29.h
    smopa za3.s, p0/m, p1/m, z30.h, z31.h
    subs x0, x0, #1
    smopa za0.s, p0/m, p1/m, z8.h,  z9.h
    smopa za1.s, p0/m, p1/m, z10.h, z11.h
    smopa za2.s, p0/m, p1/m, z12.h, z13.h
    smopa za3.s, p0/m, p1/m, z14.h, z15.h
    smopa za0.s, p0/m, p1/m, z16.h, z17.h
    smopa za1.s, p0/m, p1/m, z18.h, z19.h
    smopa za2.s, p0/m, p1/m, z20.h, z21.h
    smopa za3.s, p0/m, p1/m, z22.h, z23.h
    smopa za0.s, p0/m, p1/m, z24.h, z25.h
    smopa za1.s, p0/m, p1/m, z26.h, z27.h
    smopa za2.s, p0/m, p1/m, z28.h, z29.h
    smopa za3.s, p0/m, p1/m, z30.h, z31.h
    bne .sme_smopa_vv_i32i16i16L1
    restore_caller_vec
    smstop
    ret

############
    PROLOGUE sme_umopa_vv_i32i16i16
    preserve_caller_vec
    smstart
    SME_I16_INIT
.sme_umopa_vv_i32i16i16L1:
    umopa za0.s, p0/m, p1/m, z8.h,  z9.h
    umopa za1.s, p0/m, p1/m, z10.h, z11.h
    umopa za2.s, p0/m, p1/m, z12.h, z13.h
    umopa za3.s, p0/m, p1/m, z14.h, z15.h
    umopa za0.s, p0/m, p1/m, z16.h, z17.h
    umopa za1.s, p0/m, p1/m, z18.h, z19.h
    umopa za2.s, p0/m, p1/m, z20.h, z21.h
    umopa za3.s, p0/m, p1/m, z22.h, z23.h
    umopa za0.s, p0/m, p1/m, z24.h, z25.h
    umopa za1.s, p0/m, p1/m, z26.h, z27.h
    umopa za2.s, p0/m, p1/m, z28.h, z29.h
    umopa za3.s, p0/m, p1/m, z30.h, z31.h
    subs x0, x0, #1
    umopa za0.s, p0/m, p1/m, z8.h,  z9.h
    umopa za1.s, p0/m, p1/m, z10.h, z11.h
    umopa za2.s, p0/m, p1/m, z12.h, z13.h
    umopa za3.s, p0/m, p1/m, z14.h, z15.h
    umopa za0.s, p0/m, p1/m, z16.h, z17.h
    umopa za1.s, p0/m, p1/m, z18.h, z19.h
    umopa za2.s, p0/m, p1/m, z20.h, z21.h
    umopa za3.s, p0/m, p1/m, z22.h, z23.h
    umopa za0.s, p0/m, p1/m, z24.h, z25.h
    umopa za1.s, p0/m, p1/m, z26.h, z27.h
    umopa za2.s, p0/m, p1/m, z28.h, z29.h
    umopa za3.s, p0/m, p1/m, z30.h, z31.h
    bne .sme_umopa_vv_i32i16i16L1
    restore_caller_vec
    smstop
    ret

############
    PROLOGUE sme_smopa2_vv_i32i16i16
    preserve_caller_vec
    smstart
    SME_I16_INIT
.sme_smopa2_vv_i32i16i16L1:
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    subs x0, x0, #1
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    smopa za0.s, p0/m, p1/m, z8.h, z9.h
    bne .sme_smopa2_vv_i32i16i16L1
    restore_caller_vec
    smstop
    ret

############
    PROLOGUE sme_umopa2_vv_i32i16i16
    preserve_caller_vec
    smstart
    SME_I16_INIT
.sme_umopa2_vv_i32i16i16L1:
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    subs x0, x0, #1
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    umopa za0.s, p0/m, p1/m, z8.h, z9.h
    bne .sme_umopa2_vv_i32i16i16L1
    restore_caller_vec
    smstop
    ret
