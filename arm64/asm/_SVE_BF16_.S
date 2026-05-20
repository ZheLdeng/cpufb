.align 5
#include "common_arm64.h"

// SVE BF16 kernels
// All kernels use 24 unrolled instructions per loop, matching the NEON
// _BF16_.S layout. The runtime FLOPS are scaled by svcntb() in cpufb.cpp,
// so comp_pl below is the "VL=128b equivalent / 16" (same convention as
// the SVE FMLA / SVE I8MM kernels).
//
// Throughput per instruction at VL=128b (NEON-equivalent):
//   bfmmla z.s, z.h, z.h        : 2x4x2 outer-product = 16 FLOPs
//   bfdot  z.s, z.h, z.h        : 4 lanes * 2-element dot * 2 = 8 FLOPs
//   bfdot  z.s, z.h, z0.h[idx]  : 4 lanes * 2-element dot * 2 = 8 FLOPs
// These are then scaled by (svcntb()/16) at runtime.

#void sve_bfmmla_f32bf16bf16(int64_t);
#void sve_bfdot_vv_f32bf16bf16(int64_t);
#void sve_bfdot_vs_f32bf16bf16(int64_t);

.macro SVE_BF16_INIT
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
    PROLOGUE sve_bfmmla_f32bf16bf16
    preserve_caller_vec
    ptrue p0.s
    SVE_BF16_INIT
.sve_bfmmla_f32bf16bf16L1:
    bfmmla z8.s,  z0.h, z1.h
    bfmmla z9.s,  z0.h, z1.h
    bfmmla z10.s, z0.h, z1.h
    bfmmla z11.s, z0.h, z1.h
    bfmmla z12.s, z0.h, z1.h
    bfmmla z13.s, z0.h, z1.h
    bfmmla z14.s, z0.h, z1.h
    bfmmla z15.s, z0.h, z1.h
    bfmmla z16.s, z0.h, z1.h
    bfmmla z17.s, z0.h, z1.h
    bfmmla z18.s, z0.h, z1.h
    bfmmla z19.s, z0.h, z1.h
    subs x0, x0, #1
    bfmmla z20.s, z0.h, z1.h
    bfmmla z21.s, z0.h, z1.h
    bfmmla z22.s, z0.h, z1.h
    bfmmla z23.s, z0.h, z1.h
    bfmmla z24.s, z0.h, z1.h
    bfmmla z25.s, z0.h, z1.h
    bfmmla z26.s, z0.h, z1.h
    bfmmla z27.s, z0.h, z1.h
    bfmmla z28.s, z0.h, z1.h
    bfmmla z29.s, z0.h, z1.h
    bfmmla z30.s, z0.h, z1.h
    bfmmla z31.s, z0.h, z1.h
    bne .sve_bfmmla_f32bf16bf16L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_bfdot_vv_f32bf16bf16
    preserve_caller_vec
    ptrue p0.s
    SVE_BF16_INIT
.sve_bfdot_vv_f32bf16bf16L1:
    bfdot z8.s,  z0.h, z1.h
    bfdot z9.s,  z0.h, z1.h
    bfdot z10.s, z0.h, z1.h
    bfdot z11.s, z0.h, z1.h
    bfdot z12.s, z0.h, z1.h
    bfdot z13.s, z0.h, z1.h
    bfdot z14.s, z0.h, z1.h
    bfdot z15.s, z0.h, z1.h
    bfdot z16.s, z0.h, z1.h
    bfdot z17.s, z0.h, z1.h
    bfdot z18.s, z0.h, z1.h
    bfdot z19.s, z0.h, z1.h
    subs x0, x0, #1
    bfdot z20.s, z0.h, z1.h
    bfdot z21.s, z0.h, z1.h
    bfdot z22.s, z0.h, z1.h
    bfdot z23.s, z0.h, z1.h
    bfdot z24.s, z0.h, z1.h
    bfdot z25.s, z0.h, z1.h
    bfdot z26.s, z0.h, z1.h
    bfdot z27.s, z0.h, z1.h
    bfdot z28.s, z0.h, z1.h
    bfdot z29.s, z0.h, z1.h
    bfdot z30.s, z0.h, z1.h
    bfdot z31.s, z0.h, z1.h
    bne .sve_bfdot_vv_f32bf16bf16L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_bfdot_vs_f32bf16bf16
    preserve_caller_vec
    ptrue p0.s
    SVE_BF16_INIT
.sve_bfdot_vs_f32bf16bf16L1:
    bfdot z8.s,  z1.h, z0.h[0]
    bfdot z9.s,  z1.h, z0.h[0]
    bfdot z10.s, z1.h, z0.h[0]
    bfdot z11.s, z1.h, z0.h[0]
    bfdot z12.s, z1.h, z0.h[0]
    bfdot z13.s, z1.h, z0.h[0]
    bfdot z14.s, z1.h, z0.h[0]
    bfdot z15.s, z1.h, z0.h[0]
    bfdot z16.s, z1.h, z0.h[0]
    bfdot z17.s, z1.h, z0.h[0]
    bfdot z18.s, z1.h, z0.h[0]
    bfdot z19.s, z1.h, z0.h[0]
    subs x0, x0, #1
    bfdot z20.s, z1.h, z0.h[0]
    bfdot z21.s, z1.h, z0.h[0]
    bfdot z22.s, z1.h, z0.h[0]
    bfdot z23.s, z1.h, z0.h[0]
    bfdot z24.s, z1.h, z0.h[0]
    bfdot z25.s, z1.h, z0.h[0]
    bfdot z26.s, z1.h, z0.h[0]
    bfdot z27.s, z1.h, z0.h[0]
    bfdot z28.s, z1.h, z0.h[0]
    bfdot z29.s, z1.h, z0.h[0]
    bfdot z30.s, z1.h, z0.h[0]
    bfdot z31.s, z1.h, z0.h[0]
    bne .sve_bfdot_vs_f32bf16bf16L1
    restore_caller_vec
    ret

############
// Latency variants: all 24 instructions write to z8 (Pattern A).
############
    PROLOGUE sve_bfmmla_f32bf16bf16_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_BF16_INIT
.sve_bfmmla_f32bf16bf16_lat_L1:
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    subs x0, x0, #1
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bfmmla z8.s, z0.h, z1.h
    bne .sve_bfmmla_f32bf16bf16_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_bfdot_vv_f32bf16bf16_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_BF16_INIT
.sve_bfdot_vv_f32bf16bf16_lat_L1:
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    subs x0, x0, #1
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bfdot z8.s, z0.h, z1.h
    bne .sve_bfdot_vv_f32bf16bf16_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_bfdot_vs_f32bf16bf16_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_BF16_INIT
.sve_bfdot_vs_f32bf16bf16_lat_L1:
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    subs x0, x0, #1
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bfdot z8.s, z1.h, z0.h[0]
    bne .sve_bfdot_vs_f32bf16bf16_lat_L1
    restore_caller_vec
    ret
