.align 5
#include "common_arm64.h"

// FEAT_SVE FP16 FMLA — fmla z.h, p/m, z.h, z.h  and  fmla z.h, z.h, z.h[idx]
// Per-instruction at VL=128b (8 fp16 lanes): 8*2 = 16 FLOPs.
// 24 unrolled inst -> 24 FLOPs/iter at registration; cpufb.cpp scales by
// svcntb() at runtime (registration value = 24*16/16).

.macro SVE_FP16_INIT
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
.endm

#void sve_fmla_vv_f16f16f16(int64_t);
#void sve_fmla_vs_f16f16f16(int64_t);
#void sve_fmla_vv_f16f16f16_latency(int64_t);

############
    PROLOGUE sve_fmla_vv_f16f16f16
    preserve_caller_vec
    ptrue p0.h
    SVE_FP16_INIT
.sve_fmla_vv_f16f16f16L1:
    fmla z8.h,  p0/m, z0.h, z1.h
    fmla z9.h,  p0/m, z0.h, z1.h
    fmla z10.h, p0/m, z0.h, z1.h
    fmla z11.h, p0/m, z0.h, z1.h
    fmla z12.h, p0/m, z0.h, z1.h
    fmla z13.h, p0/m, z0.h, z1.h
    fmla z14.h, p0/m, z0.h, z1.h
    fmla z15.h, p0/m, z0.h, z1.h
    fmla z16.h, p0/m, z0.h, z1.h
    fmla z17.h, p0/m, z0.h, z1.h
    fmla z18.h, p0/m, z0.h, z1.h
    fmla z19.h, p0/m, z0.h, z1.h
    subs x0, x0, #1
    fmla z20.h, p0/m, z0.h, z1.h
    fmla z21.h, p0/m, z0.h, z1.h
    fmla z22.h, p0/m, z0.h, z1.h
    fmla z23.h, p0/m, z0.h, z1.h
    fmla z24.h, p0/m, z0.h, z1.h
    fmla z25.h, p0/m, z0.h, z1.h
    fmla z26.h, p0/m, z0.h, z1.h
    fmla z27.h, p0/m, z0.h, z1.h
    fmla z28.h, p0/m, z0.h, z1.h
    fmla z29.h, p0/m, z0.h, z1.h
    fmla z30.h, p0/m, z0.h, z1.h
    fmla z31.h, p0/m, z0.h, z1.h
    bne .sve_fmla_vv_f16f16f16L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_fmla_vs_f16f16f16
    preserve_caller_vec
    ptrue p0.h
    SVE_FP16_INIT
.sve_fmla_vs_f16f16f16L1:
    fmla z8.h,  z0.h, z1.h[0]
    fmla z9.h,  z0.h, z1.h[0]
    fmla z10.h, z0.h, z1.h[0]
    fmla z11.h, z0.h, z1.h[0]
    fmla z12.h, z0.h, z1.h[0]
    fmla z13.h, z0.h, z1.h[0]
    fmla z14.h, z0.h, z1.h[0]
    fmla z15.h, z0.h, z1.h[0]
    fmla z16.h, z0.h, z1.h[0]
    fmla z17.h, z0.h, z1.h[0]
    fmla z18.h, z0.h, z1.h[0]
    fmla z19.h, z0.h, z1.h[0]
    subs x0, x0, #1
    fmla z20.h, z0.h, z1.h[0]
    fmla z21.h, z0.h, z1.h[0]
    fmla z22.h, z0.h, z1.h[0]
    fmla z23.h, z0.h, z1.h[0]
    fmla z24.h, z0.h, z1.h[0]
    fmla z25.h, z0.h, z1.h[0]
    fmla z26.h, z0.h, z1.h[0]
    fmla z27.h, z0.h, z1.h[0]
    fmla z28.h, z0.h, z1.h[0]
    fmla z29.h, z0.h, z1.h[0]
    fmla z30.h, z0.h, z1.h[0]
    fmla z31.h, z0.h, z1.h[0]
    bne .sve_fmla_vs_f16f16f16L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_fmla_vv_f16f16f16_latency
    preserve_caller_vec
    ptrue p0.h
    SVE_FP16_INIT
.sve_fmla_vv_f16f16f16_latencyL1:
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    subs x0, x0, #1
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    fmla z8.h, p0/m, z0.h, z1.h
    bne .sve_fmla_vv_f16f16f16_latencyL1
    restore_caller_vec
    ret
