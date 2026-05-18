.align 5
#include "common_arm64.h"

// FEAT_SME_F16F16 — fmopa za.h, p/m, p/m, z.h, z.h
// fp16 outer-product accumulating into a fp16 ZA tile.
// Per-instruction at SVL=N bytes: (N/2) x (N/2) outer product * 2 ops
//   = (rdsvl()^2) / 2  FLOPs.
// Register comp_pl = 24 (24 instructions * 1 unit), then cpufb.cpp scales by
// rdsvl()*rdsvl()/2/2 in the SME comp_pl scaling block (16-bit accumulator
// case added there).

.macro SME_F16F16_INIT
    fmov z0.h, p0/m, #0
    fmov z1.h, p0/m, #0
    fmov z8.h, p0/m, #0
    fmov z9.h, p0/m, #0
    fmov z10.h, p0/m, #0
    fmov z11.h, p0/m, #0
    fmov z12.h, p0/m, #0
    fmov z13.h, p0/m, #0
    fmov z14.h, p0/m, #0
    fmov z15.h, p0/m, #0
    fmov z16.h, p0/m, #0
    fmov z17.h, p0/m, #0
    fmov z18.h, p0/m, #0
    fmov z19.h, p0/m, #0
    fmov z20.h, p0/m, #0
    fmov z21.h, p0/m, #0
    fmov z22.h, p0/m, #0
    fmov z23.h, p0/m, #0
    fmov z24.h, p0/m, #0
    fmov z25.h, p0/m, #0
    fmov z26.h, p0/m, #0
    fmov z27.h, p0/m, #0
    fmov z28.h, p0/m, #0
    fmov z29.h, p0/m, #0
    fmov z30.h, p0/m, #0
    fmov z31.h, p0/m, #0
    ptrue p0.h
    ptrue p1.h
.endm

#void sme_fmopa_vv_f16f16f16(int64_t);
#void sme_fmopa2_vv_f16f16f16(int64_t);  // latency: single ZA tile

############
    PROLOGUE sme_fmopa_vv_f16f16f16
    preserve_caller_vec
    smstart
    SME_F16F16_INIT
.sme_fmopa_vv_f16f16f16L1:
    fmopa za0.h, p0/m, p1/m, z8.h,  z9.h
    fmopa za1.h, p0/m, p1/m, z10.h, z11.h
    fmopa za0.h, p0/m, p1/m, z12.h, z13.h
    fmopa za1.h, p0/m, p1/m, z14.h, z15.h
    fmopa za0.h, p0/m, p1/m, z16.h, z17.h
    fmopa za1.h, p0/m, p1/m, z18.h, z19.h
    fmopa za0.h, p0/m, p1/m, z20.h, z21.h
    fmopa za1.h, p0/m, p1/m, z22.h, z23.h
    fmopa za0.h, p0/m, p1/m, z24.h, z25.h
    fmopa za1.h, p0/m, p1/m, z26.h, z27.h
    fmopa za0.h, p0/m, p1/m, z28.h, z29.h
    fmopa za1.h, p0/m, p1/m, z30.h, z31.h
    subs x0, x0, #1
    fmopa za0.h, p0/m, p1/m, z8.h,  z9.h
    fmopa za1.h, p0/m, p1/m, z10.h, z11.h
    fmopa za0.h, p0/m, p1/m, z12.h, z13.h
    fmopa za1.h, p0/m, p1/m, z14.h, z15.h
    fmopa za0.h, p0/m, p1/m, z16.h, z17.h
    fmopa za1.h, p0/m, p1/m, z18.h, z19.h
    fmopa za0.h, p0/m, p1/m, z20.h, z21.h
    fmopa za1.h, p0/m, p1/m, z22.h, z23.h
    fmopa za0.h, p0/m, p1/m, z24.h, z25.h
    fmopa za1.h, p0/m, p1/m, z26.h, z27.h
    fmopa za0.h, p0/m, p1/m, z28.h, z29.h
    fmopa za1.h, p0/m, p1/m, z30.h, z31.h
    bne .sme_fmopa_vv_f16f16f16L1
    restore_caller_vec
    smstop
    ret

############
    PROLOGUE sme_fmopa2_vv_f16f16f16
    preserve_caller_vec
    smstart
    SME_F16F16_INIT
.sme_fmopa2_vv_f16f16f16L1:
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    subs x0, x0, #1
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    fmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bne .sme_fmopa2_vv_f16f16f16L1
    restore_caller_vec
    smstop
    ret
