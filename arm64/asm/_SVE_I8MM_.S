.align 5
#include "common_arm64.h"

// SVE I8MM kernels
// All kernels use 24 unrolled instructions per loop, matching the NEON
// _I8MM_.S layout. The runtime FLOPS are scaled by svcntb() in cpufb.cpp,
// so comp_pl below is the "VL=128b equivalent / 16" (same convention as
// the SVE FMLA kernels in _SVE_.S).

#void sve_mmla_s32s8s8(int64_t);
#void sve_mmla_u32u8u8(int64_t);
#void sve_mmla_s32u8s8(int64_t);
#void sve_dp4a_vv_s32s8s8(int64_t);
#void sve_dp4a_vv_s32u8u8(int64_t);
#void sve_dp4a_vv_s32u8s8(int64_t);

.macro SVE_I8_INIT
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
    PROLOGUE sve_mmla_s32s8s8
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_mmla_s32s8s8L1:
    smmla z8.s,  z0.b, z1.b
    smmla z9.s,  z0.b, z1.b
    smmla z10.s, z0.b, z1.b
    smmla z11.s, z0.b, z1.b
    smmla z12.s, z0.b, z1.b
    smmla z13.s, z0.b, z1.b
    smmla z14.s, z0.b, z1.b
    smmla z15.s, z0.b, z1.b
    smmla z16.s, z0.b, z1.b
    smmla z17.s, z0.b, z1.b
    smmla z18.s, z0.b, z1.b
    smmla z19.s, z0.b, z1.b
    subs x0, x0, #1
    smmla z20.s, z0.b, z1.b
    smmla z21.s, z0.b, z1.b
    smmla z22.s, z0.b, z1.b
    smmla z23.s, z0.b, z1.b
    smmla z24.s, z0.b, z1.b
    smmla z25.s, z0.b, z1.b
    smmla z26.s, z0.b, z1.b
    smmla z27.s, z0.b, z1.b
    smmla z28.s, z0.b, z1.b
    smmla z29.s, z0.b, z1.b
    smmla z30.s, z0.b, z1.b
    smmla z31.s, z0.b, z1.b
    bne .sve_mmla_s32s8s8L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_mmla_u32u8u8
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_mmla_u32u8u8L1:
    ummla z8.s,  z0.b, z1.b
    ummla z9.s,  z0.b, z1.b
    ummla z10.s, z0.b, z1.b
    ummla z11.s, z0.b, z1.b
    ummla z12.s, z0.b, z1.b
    ummla z13.s, z0.b, z1.b
    ummla z14.s, z0.b, z1.b
    ummla z15.s, z0.b, z1.b
    ummla z16.s, z0.b, z1.b
    ummla z17.s, z0.b, z1.b
    ummla z18.s, z0.b, z1.b
    ummla z19.s, z0.b, z1.b
    subs x0, x0, #1
    ummla z20.s, z0.b, z1.b
    ummla z21.s, z0.b, z1.b
    ummla z22.s, z0.b, z1.b
    ummla z23.s, z0.b, z1.b
    ummla z24.s, z0.b, z1.b
    ummla z25.s, z0.b, z1.b
    ummla z26.s, z0.b, z1.b
    ummla z27.s, z0.b, z1.b
    ummla z28.s, z0.b, z1.b
    ummla z29.s, z0.b, z1.b
    ummla z30.s, z0.b, z1.b
    ummla z31.s, z0.b, z1.b
    bne .sve_mmla_u32u8u8L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_mmla_s32u8s8
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_mmla_s32u8s8L1:
    usmmla z8.s,  z0.b, z1.b
    usmmla z9.s,  z0.b, z1.b
    usmmla z10.s, z0.b, z1.b
    usmmla z11.s, z0.b, z1.b
    usmmla z12.s, z0.b, z1.b
    usmmla z13.s, z0.b, z1.b
    usmmla z14.s, z0.b, z1.b
    usmmla z15.s, z0.b, z1.b
    usmmla z16.s, z0.b, z1.b
    usmmla z17.s, z0.b, z1.b
    usmmla z18.s, z0.b, z1.b
    usmmla z19.s, z0.b, z1.b
    subs x0, x0, #1
    usmmla z20.s, z0.b, z1.b
    usmmla z21.s, z0.b, z1.b
    usmmla z22.s, z0.b, z1.b
    usmmla z23.s, z0.b, z1.b
    usmmla z24.s, z0.b, z1.b
    usmmla z25.s, z0.b, z1.b
    usmmla z26.s, z0.b, z1.b
    usmmla z27.s, z0.b, z1.b
    usmmla z28.s, z0.b, z1.b
    usmmla z29.s, z0.b, z1.b
    usmmla z30.s, z0.b, z1.b
    usmmla z31.s, z0.b, z1.b
    bne .sve_mmla_s32u8s8L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_dp4a_vv_s32s8s8
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_dp4a_vv_s32s8s8L1:
    sdot z8.s,  z0.b, z1.b
    sdot z9.s,  z0.b, z1.b
    sdot z10.s, z0.b, z1.b
    sdot z11.s, z0.b, z1.b
    sdot z12.s, z0.b, z1.b
    sdot z13.s, z0.b, z1.b
    sdot z14.s, z0.b, z1.b
    sdot z15.s, z0.b, z1.b
    sdot z16.s, z0.b, z1.b
    sdot z17.s, z0.b, z1.b
    sdot z18.s, z0.b, z1.b
    sdot z19.s, z0.b, z1.b
    subs x0, x0, #1
    sdot z20.s, z0.b, z1.b
    sdot z21.s, z0.b, z1.b
    sdot z22.s, z0.b, z1.b
    sdot z23.s, z0.b, z1.b
    sdot z24.s, z0.b, z1.b
    sdot z25.s, z0.b, z1.b
    sdot z26.s, z0.b, z1.b
    sdot z27.s, z0.b, z1.b
    sdot z28.s, z0.b, z1.b
    sdot z29.s, z0.b, z1.b
    sdot z30.s, z0.b, z1.b
    sdot z31.s, z0.b, z1.b
    bne .sve_dp4a_vv_s32s8s8L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_dp4a_vv_s32u8u8
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_dp4a_vv_s32u8u8L1:
    udot z8.s,  z0.b, z1.b
    udot z9.s,  z0.b, z1.b
    udot z10.s, z0.b, z1.b
    udot z11.s, z0.b, z1.b
    udot z12.s, z0.b, z1.b
    udot z13.s, z0.b, z1.b
    udot z14.s, z0.b, z1.b
    udot z15.s, z0.b, z1.b
    udot z16.s, z0.b, z1.b
    udot z17.s, z0.b, z1.b
    udot z18.s, z0.b, z1.b
    udot z19.s, z0.b, z1.b
    subs x0, x0, #1
    udot z20.s, z0.b, z1.b
    udot z21.s, z0.b, z1.b
    udot z22.s, z0.b, z1.b
    udot z23.s, z0.b, z1.b
    udot z24.s, z0.b, z1.b
    udot z25.s, z0.b, z1.b
    udot z26.s, z0.b, z1.b
    udot z27.s, z0.b, z1.b
    udot z28.s, z0.b, z1.b
    udot z29.s, z0.b, z1.b
    udot z30.s, z0.b, z1.b
    udot z31.s, z0.b, z1.b
    bne .sve_dp4a_vv_s32u8u8L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_dp4a_vv_s32u8s8
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_dp4a_vv_s32u8s8L1:
    usdot z8.s,  z0.b, z1.b
    usdot z9.s,  z0.b, z1.b
    usdot z10.s, z0.b, z1.b
    usdot z11.s, z0.b, z1.b
    usdot z12.s, z0.b, z1.b
    usdot z13.s, z0.b, z1.b
    usdot z14.s, z0.b, z1.b
    usdot z15.s, z0.b, z1.b
    usdot z16.s, z0.b, z1.b
    usdot z17.s, z0.b, z1.b
    usdot z18.s, z0.b, z1.b
    usdot z19.s, z0.b, z1.b
    subs x0, x0, #1
    usdot z20.s, z0.b, z1.b
    usdot z21.s, z0.b, z1.b
    usdot z22.s, z0.b, z1.b
    usdot z23.s, z0.b, z1.b
    usdot z24.s, z0.b, z1.b
    usdot z25.s, z0.b, z1.b
    usdot z26.s, z0.b, z1.b
    usdot z27.s, z0.b, z1.b
    usdot z28.s, z0.b, z1.b
    usdot z29.s, z0.b, z1.b
    usdot z30.s, z0.b, z1.b
    usdot z31.s, z0.b, z1.b
    bne .sve_dp4a_vv_s32u8s8L1
    restore_caller_vec
    ret

############
// Latency variants: all 24 instructions write to z8 (Pattern A).
############
    PROLOGUE sve_mmla_s32s8s8_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_mmla_s32s8s8_lat_L1:
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    subs x0, x0, #1
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    smmla z8.s, z0.b, z1.b
    bne .sve_mmla_s32s8s8_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_mmla_u32u8u8_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_mmla_u32u8u8_lat_L1:
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    subs x0, x0, #1
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    ummla z8.s, z0.b, z1.b
    bne .sve_mmla_u32u8u8_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_mmla_s32u8s8_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_mmla_s32u8s8_lat_L1:
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    subs x0, x0, #1
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    usmmla z8.s, z0.b, z1.b
    bne .sve_mmla_s32u8s8_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_dp4a_vv_s32s8s8_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_dp4a_vv_s32s8s8_lat_L1:
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    subs x0, x0, #1
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    sdot z8.s, z0.b, z1.b
    bne .sve_dp4a_vv_s32s8s8_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_dp4a_vv_s32u8u8_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_dp4a_vv_s32u8u8_lat_L1:
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    subs x0, x0, #1
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    udot z8.s, z0.b, z1.b
    bne .sve_dp4a_vv_s32u8u8_lat_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_dp4a_vv_s32u8s8_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_I8_INIT
.sve_dp4a_vv_s32u8s8_lat_L1:
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    subs x0, x0, #1
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    usdot z8.s, z0.b, z1.b
    bne .sve_dp4a_vv_s32u8s8_lat_L1
    restore_caller_vec
    ret
