.align 5
#include "common_arm64.h"

// FEAT_SVE2 — representative integer MAC: sqrdmlah z.h, z.h, z.h
// (signed saturating rounding doubling multiply-add returning high half)
// Per-instruction at VL=128b (8 i16 lanes): 8*2 = 16 OPs.
// 24 unrolled inst -> register 24 (24 * 16 / 16 = 24); cpufb.cpp scales
// by svcntb() at runtime.

.macro SVE2_INIT
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

#void sve2_sqrdmlah_vv_s16s16s16(int64_t);
#void sve2_sqrdmlah_vv_s16s16s16_latency(int64_t);

############
    PROLOGUE sve2_sqrdmlah_vv_s16s16s16
    preserve_caller_vec
    ptrue p0.h
    SVE2_INIT
.sve2_sqrdmlah_vv_s16s16s16L1:
    sqrdmlah z8.h,  z0.h, z1.h
    sqrdmlah z9.h,  z0.h, z1.h
    sqrdmlah z10.h, z0.h, z1.h
    sqrdmlah z11.h, z0.h, z1.h
    sqrdmlah z12.h, z0.h, z1.h
    sqrdmlah z13.h, z0.h, z1.h
    sqrdmlah z14.h, z0.h, z1.h
    sqrdmlah z15.h, z0.h, z1.h
    sqrdmlah z16.h, z0.h, z1.h
    sqrdmlah z17.h, z0.h, z1.h
    sqrdmlah z18.h, z0.h, z1.h
    sqrdmlah z19.h, z0.h, z1.h
    subs x0, x0, #1
    sqrdmlah z20.h, z0.h, z1.h
    sqrdmlah z21.h, z0.h, z1.h
    sqrdmlah z22.h, z0.h, z1.h
    sqrdmlah z23.h, z0.h, z1.h
    sqrdmlah z24.h, z0.h, z1.h
    sqrdmlah z25.h, z0.h, z1.h
    sqrdmlah z26.h, z0.h, z1.h
    sqrdmlah z27.h, z0.h, z1.h
    sqrdmlah z28.h, z0.h, z1.h
    sqrdmlah z29.h, z0.h, z1.h
    sqrdmlah z30.h, z0.h, z1.h
    sqrdmlah z31.h, z0.h, z1.h
    bne .sve2_sqrdmlah_vv_s16s16s16L1
    restore_caller_vec
    ret

############
    PROLOGUE sve2_sqrdmlah_vv_s16s16s16_latency
    preserve_caller_vec
    ptrue p0.h
    SVE2_INIT
.sve2_sqrdmlah_vv_s16s16s16_latencyL1:
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    subs x0, x0, #1
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    sqrdmlah z8.h, z0.h, z1.h
    bne .sve2_sqrdmlah_vv_s16s16s16_latencyL1
    restore_caller_vec
    ret
