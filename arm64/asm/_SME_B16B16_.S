.align 5
#include "common_arm64.h"

// FEAT_SME_B16B16 — bfmopa za.h, p/m, p/m, z.h, z.h  (non-widening)
// bf16 outer-product accumulating into a bf16 ZA tile.
// Per-instruction at SVL=N bytes: (N/2) x (N/2) outer product * 2 ops
//   = (rdsvl()^2) / 2  FLOPs.
// Contrast with the widening BFMOPA in _SME_.S (za.s, f32 accumulator), which
// does (N/4) x (N/4) x 2-way dot = (rdsvl()^2) / 4 FLOPs per instruction.
// Register comp_pl = 48 (24 instructions * 2 units); cpufb.cpp then scales by
// rdsvl()*rdsvl()/2/2 in the 16-bit-accumulator branch of the SME comp_pl
// scaling block.

.macro SME_B16B16_INIT
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

#void sme_bfmopa_vv_bf16bf16bf16(int64_t);
#void sme_bfmopa2_vv_bf16bf16bf16(int64_t);  // latency: single ZA tile

############
    PROLOGUE sme_bfmopa_vv_bf16bf16bf16
    preserve_caller_vec
    smstart
    SME_B16B16_INIT
.sme_bfmopa_vv_bf16bf16bf16L1:
    bfmopa za0.h, p0/m, p1/m, z8.h,  z9.h
    bfmopa za1.h, p0/m, p1/m, z10.h, z11.h
    bfmopa za0.h, p0/m, p1/m, z12.h, z13.h
    bfmopa za1.h, p0/m, p1/m, z14.h, z15.h
    bfmopa za0.h, p0/m, p1/m, z16.h, z17.h
    bfmopa za1.h, p0/m, p1/m, z18.h, z19.h
    bfmopa za0.h, p0/m, p1/m, z20.h, z21.h
    bfmopa za1.h, p0/m, p1/m, z22.h, z23.h
    bfmopa za0.h, p0/m, p1/m, z24.h, z25.h
    bfmopa za1.h, p0/m, p1/m, z26.h, z27.h
    bfmopa za0.h, p0/m, p1/m, z28.h, z29.h
    bfmopa za1.h, p0/m, p1/m, z30.h, z31.h
    subs x0, x0, #1
    bfmopa za0.h, p0/m, p1/m, z8.h,  z9.h
    bfmopa za1.h, p0/m, p1/m, z10.h, z11.h
    bfmopa za0.h, p0/m, p1/m, z12.h, z13.h
    bfmopa za1.h, p0/m, p1/m, z14.h, z15.h
    bfmopa za0.h, p0/m, p1/m, z16.h, z17.h
    bfmopa za1.h, p0/m, p1/m, z18.h, z19.h
    bfmopa za0.h, p0/m, p1/m, z20.h, z21.h
    bfmopa za1.h, p0/m, p1/m, z22.h, z23.h
    bfmopa za0.h, p0/m, p1/m, z24.h, z25.h
    bfmopa za1.h, p0/m, p1/m, z26.h, z27.h
    bfmopa za0.h, p0/m, p1/m, z28.h, z29.h
    bfmopa za1.h, p0/m, p1/m, z30.h, z31.h
    bne .sme_bfmopa_vv_bf16bf16bf16L1
    smstop
    restore_caller_vec
    ret

############
    PROLOGUE sme_bfmopa2_vv_bf16bf16bf16
    preserve_caller_vec
    smstart
    SME_B16B16_INIT
.sme_bfmopa2_vv_bf16bf16bf16L1:
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    subs x0, x0, #1
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bfmopa za0.h, p0/m, p1/m, z8.h, z9.h
    bne .sme_bfmopa2_vv_bf16bf16bf16L1
    smstop
    restore_caller_vec
    ret
