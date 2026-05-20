.align 5
#include "common_arm64.h"

############
#uint64_t load_sve_vector_bytes(void);
    PROLOGUE load_sve_vector_bytes
    cntb x0
    ret

.macro SVE_INIT
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

    PROLOGUE sve_fmla_vs_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vs_f32f32f32L1:
    fmla z8.s, z0.s, z1.s[0]
    fmla z9.s, z0.s, z1.s[0]
    fmla z10.s, z0.s, z1.s[0]
    fmla z11.s, z0.s, z1.s[0]
    fmla z12.s, z0.s, z1.s[0]
    fmla z13.s, z0.s, z1.s[0]
    fmla z14.s, z0.s, z1.s[0]
    fmla z15.s, z0.s, z1.s[0]
    fmla z16.s, z0.s, z1.s[0]
    fmla z17.s, z0.s, z1.s[0]
    fmla z18.s, z0.s, z1.s[0]
    fmla z19.s, z0.s, z1.s[0]
    subs x0, x0, #1
    fmla z20.s, z0.s, z1.s[0]
    fmla z21.s, z0.s, z1.s[0]
    fmla z22.s, z0.s, z1.s[0]
    fmla z23.s, z0.s, z1.s[0]
    fmla z24.s, z0.s, z1.s[0]
    fmla z25.s, z0.s, z1.s[0]
    fmla z26.s, z0.s, z1.s[0]
    fmla z27.s, z0.s, z1.s[0]
    fmla z28.s, z0.s, z1.s[0]
    fmla z29.s, z0.s, z1.s[0]
    fmla z30.s, z0.s, z1.s[0]
    fmla z31.s, z0.s, z1.s[0]
    bne .sve_fmla_vs_f32f32f32L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_fmla_vv_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vv_f32f32f32L1:
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z9.s, p0/m, z9.s, z9.s
    fmla z10.s, p0/m, z10.s, z10.s
    fmla z11.s, p0/m, z11.s, z11.s
    fmla z12.s, p0/m, z12.s, z12.s
    fmla z13.s, p0/m, z13.s, z13.s
    fmla z14.s, p0/m, z14.s, z14.s
    fmla z15.s, p0/m, z15.s, z15.s
    fmla z16.s, p0/m, z16.s, z16.s
    fmla z17.s, p0/m, z17.s, z17.s
    fmla z18.s, p0/m, z18.s, z18.s
    fmla z19.s, p0/m, z19.s, z19.s
    subs x0, x0, #1
    fmla z20.s, p0/m, z20.s, z20.s
    fmla z21.s, p0/m, z21.s, z21.s
    fmla z22.s, p0/m, z22.s, z22.s
    fmla z23.s, p0/m, z23.s, z23.s
    fmla z24.s, p0/m, z24.s, z24.s
    fmla z25.s, p0/m, z25.s, z25.s
    fmla z26.s, p0/m, z26.s, z26.s
    fmla z27.s, p0/m, z27.s, z27.s
    fmla z28.s, p0/m, z28.s, z28.s
    fmla z29.s, p0/m, z29.s, z29.s
    fmla z30.s, p0/m, z30.s, z30.s
    fmla z31.s, p0/m, z31.s, z31.s
    bne .sve_fmla_vv_f32f32f32L1
    restore_caller_vec
    ret

############

    PROLOGUE sve_fmla_vs_f64f64f64
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vs_f64f64f64L1:
    fmla z8.d, z0.d, z1.d[0]
    fmla z9.d, z0.d, z1.d[0]
    fmla z10.d, z0.d, z1.d[0]
    fmla z11.d, z0.d, z1.d[0]
    fmla z12.d, z0.d, z1.d[0]
    fmla z13.d, z0.d, z1.d[0]
    fmla z14.d, z0.d, z1.d[0]
    fmla z15.d, z0.d, z1.d[0]
    fmla z16.d, z0.d, z1.d[0]
    fmla z17.d, z0.d, z1.d[0]
    fmla z18.d, z0.d, z1.d[0]
    fmla z19.d, z0.d, z1.d[0]
    subs x0, x0, #1
    fmla z20.d, z0.d, z1.d[0]
    fmla z21.d, z0.d, z1.d[0]
    fmla z22.d, z0.d, z1.d[0]
    fmla z23.d, z0.d, z1.d[0]
    fmla z24.d, z0.d, z1.d[0]
    fmla z25.d, z0.d, z1.d[0]
    fmla z26.d, z0.d, z1.d[0]
    fmla z27.d, z0.d, z1.d[0]
    fmla z28.d, z0.d, z1.d[0]
    fmla z29.d, z0.d, z1.d[0]
    fmla z30.d, z0.d, z1.d[0]
    fmla z31.d, z0.d, z1.d[0]
    bne .sve_fmla_vs_f64f64f64L1
    restore_caller_vec
    ret

############

    PROLOGUE sve_fmla_vv_f64f64f64
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla_vv_f64f64f64L1:
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z9.d, p0/m, z9.d, z9.d
    fmla z10.d, p0/m, z10.d, z10.d
    fmla z11.d, p0/m, z11.d, z11.d
    fmla z12.d, p0/m, z12.d, z12.d
    fmla z13.d, p0/m, z13.d, z13.d
    fmla z14.d, p0/m, z14.d, z14.d
    fmla z15.d, p0/m, z15.d, z15.d
    fmla z16.d, p0/m, z16.d, z16.d
    fmla z17.d, p0/m, z17.d, z17.d
    fmla z18.d, p0/m, z18.d, z18.d
    fmla z19.d, p0/m, z19.d, z19.d
    subs x0, x0, #1
    fmla z20.d, p0/m, z20.d, z20.d
    fmla z21.d, p0/m, z21.d, z21.d
    fmla z22.d, p0/m, z22.d, z22.d
    fmla z23.d, p0/m, z23.d, z23.d
    fmla z24.d, p0/m, z24.d, z24.d
    fmla z25.d, p0/m, z25.d, z25.d
    fmla z26.d, p0/m, z26.d, z26.d
    fmla z27.d, p0/m, z27.d, z27.d
    fmla z28.d, p0/m, z28.d, z28.d
    fmla z29.d, p0/m, z29.d, z29.d
    fmla z30.d, p0/m, z30.d, z30.d
    fmla z31.d, p0/m, z31.d, z31.d
    bne .sve_fmla_vv_f64f64f64L1
    restore_caller_vec
    ret
############
#void load_sve_ld1b_kernel(float*, int, int64_t);
    PROLOGUE load_sve_ld1b_kernel
    mov x4, x2
    ptrue p0.b
    2:
    mov x3, x1
    mov x5, x0
    1:
    mov x6, x5
    addvl x6, x6, #8
    ld1b z0.b, p0/z, [x5]
    ld1b z1.b, p0/z, [x5, #1, MUL VL]
    ld1b z2.b, p0/z, [x5, #2, MUL VL]
    ld1b z3.b, p0/z, [x5, #3, MUL VL]
    ld1b z4.b, p0/z, [x5, #4, MUL VL]
    ld1b z5.b, p0/z, [x5, #5, MUL VL]
    ld1b z6.b, p0/z, [x5, #6, MUL VL]
    ld1b z7.b, p0/z, [x5, #7, MUL VL]
    ld1b z16.b, p0/z, [x6]
    ld1b z17.b, p0/z, [x6, #1, MUL VL]
    ld1b z18.b, p0/z, [x6, #2, MUL VL]
    ld1b z19.b, p0/z, [x6, #3, MUL VL]
    ld1b z20.b, p0/z, [x6, #4, MUL VL]
    ld1b z21.b, p0/z, [x6, #5, MUL VL]
    ld1b z22.b, p0/z, [x6, #6, MUL VL]
    ld1b z23.b, p0/z, [x6, #7, MUL VL]
    addvl x5, x5, #16
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

############
#void load_sve_ld1h_kernel(float*, int, int64_t);
    PROLOGUE load_sve_ld1h_kernel
    mov x4, x2
    ptrue p0.h
    2:
    mov x3, x1
    mov x5, x0
    1:
    mov x6, x5
    addvl x6, x6, #8
    ld1h z0.h, p0/z, [x5]
    ld1h z1.h, p0/z, [x5, #1, MUL VL]
    ld1h z2.h, p0/z, [x5, #2, MUL VL]
    ld1h z3.h, p0/z, [x5, #3, MUL VL]
    ld1h z4.h, p0/z, [x5, #4, MUL VL]
    ld1h z5.h, p0/z, [x5, #5, MUL VL]
    ld1h z6.h, p0/z, [x5, #6, MUL VL]
    ld1h z7.h, p0/z, [x5, #7, MUL VL]
    ld1h z16.h, p0/z, [x6]
    ld1h z17.h, p0/z, [x6, #1, MUL VL]
    ld1h z18.h, p0/z, [x6, #2, MUL VL]
    ld1h z19.h, p0/z, [x6, #3, MUL VL]
    ld1h z20.h, p0/z, [x6, #4, MUL VL]
    ld1h z21.h, p0/z, [x6, #5, MUL VL]
    ld1h z22.h, p0/z, [x6, #6, MUL VL]
    ld1h z23.h, p0/z, [x6, #7, MUL VL]
    addvl x5, x5, #16
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

############
#void load_ld1w_kernel(float*, int, int64_t);
    PROLOGUE load_ld1w_kernel
    mov x4, x2
    ptrue p0.s
    2:
    mov x3, x1
    mov x5, x0
    1:
    mov x6, x5
    addvl x6, x6, #8
    ld1w z0.s, p0/z, [x5]
    ld1w z1.s, p0/z, [x5, #1, MUL VL]
    ld1w z2.s, p0/z, [x5, #2, MUL VL]
    ld1w z3.s, p0/z, [x5, #3, MUL VL]
    ld1w z4.s, p0/z, [x5, #4, MUL VL]
    ld1w z5.s, p0/z, [x5, #5, MUL VL]
    ld1w z6.s, p0/z, [x5, #6, MUL VL]
    ld1w z7.s, p0/z, [x5, #7, MUL VL]
    ld1w z16.s, p0/z, [x6]
    ld1w z17.s, p0/z, [x6, #1, MUL VL]
    ld1w z18.s, p0/z, [x6, #2, MUL VL]
    ld1w z19.s, p0/z, [x6, #3, MUL VL]
    ld1w z20.s, p0/z, [x6, #4, MUL VL]
    ld1w z21.s, p0/z, [x6, #5, MUL VL]
    ld1w z22.s, p0/z, [x6, #6, MUL VL]
    ld1w z23.s, p0/z, [x6, #7, MUL VL]
    addvl x5, x5, #16
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

############
#void load_sve_ld1d_kernel(float*, int, int64_t);
    PROLOGUE load_sve_ld1d_kernel
    mov x4, x2
    ptrue p0.d
    2:
    mov x3, x1
    mov x5, x0
    1:
    mov x6, x5
    addvl x6, x6, #8
    ld1d z0.d, p0/z, [x5]
    ld1d z1.d, p0/z, [x5, #1, MUL VL]
    ld1d z2.d, p0/z, [x5, #2, MUL VL]
    ld1d z3.d, p0/z, [x5, #3, MUL VL]
    ld1d z4.d, p0/z, [x5, #4, MUL VL]
    ld1d z5.d, p0/z, [x5, #5, MUL VL]
    ld1d z6.d, p0/z, [x5, #6, MUL VL]
    ld1d z7.d, p0/z, [x5, #7, MUL VL]
    ld1d z16.d, p0/z, [x6]
    ld1d z17.d, p0/z, [x6, #1, MUL VL]
    ld1d z18.d, p0/z, [x6, #2, MUL VL]
    ld1d z19.d, p0/z, [x6, #3, MUL VL]
    ld1d z20.d, p0/z, [x6, #4, MUL VL]
    ld1d z21.d, p0/z, [x6, #5, MUL VL]
    ld1d z22.d, p0/z, [x6, #6, MUL VL]
    ld1d z23.d, p0/z, [x6, #7, MUL VL]
    addvl x5, x5, #16
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret
############
    PROLOGUE sve_fmla2_vv_f32f32f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla2_vv_f32f32f32L1:
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    subs x0, x0, #1
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    fmla z8.s, p0/m, z8.s, z8.s
    bne .sve_fmla2_vv_f32f32f32L1
    restore_caller_vec
    ret

############

    PROLOGUE sve_fmla2_vv_f64f64f64
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_fmla2_vv_f64f64f64L1:
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    subs x0, x0, #1
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    fmla z8.d, p0/m, z8.d, z8.d
    bne .sve_fmla2_vv_f64f64f64L1
    restore_caller_vec
    ret

############
// SVE complex FCMLA (predicated form):
//   fcmla zda.s, p/m, zn.s, zm.s, #rot
//   fcmla zda.d, p/m, zn.d, zm.d, #rot
// Per-instruction op count is 2 ops/lane (half a complex FMA per pair).
// 24 unrolled instructions per loop -> raw comp_pl = 24 * 2 = 48; the
// cpufb.cpp SVE path multiplies by svcntb()/element_size to scale to SVL.

.macro SVE_FCMLA_F32_LOOP rot, label
.\label:
    fcmla z8.s , p0/m, z0.s, z1.s, #\rot
    fcmla z9.s , p0/m, z0.s, z1.s, #\rot
    fcmla z10.s, p0/m, z0.s, z1.s, #\rot
    fcmla z11.s, p0/m, z0.s, z1.s, #\rot
    fcmla z12.s, p0/m, z0.s, z1.s, #\rot
    fcmla z13.s, p0/m, z0.s, z1.s, #\rot
    fcmla z14.s, p0/m, z0.s, z1.s, #\rot
    fcmla z15.s, p0/m, z0.s, z1.s, #\rot
    fcmla z16.s, p0/m, z0.s, z1.s, #\rot
    fcmla z17.s, p0/m, z0.s, z1.s, #\rot
    fcmla z18.s, p0/m, z0.s, z1.s, #\rot
    fcmla z19.s, p0/m, z0.s, z1.s, #\rot
    subs x0, x0, #1
    fcmla z20.s, p0/m, z0.s, z1.s, #\rot
    fcmla z21.s, p0/m, z0.s, z1.s, #\rot
    fcmla z22.s, p0/m, z0.s, z1.s, #\rot
    fcmla z23.s, p0/m, z0.s, z1.s, #\rot
    fcmla z24.s, p0/m, z0.s, z1.s, #\rot
    fcmla z25.s, p0/m, z0.s, z1.s, #\rot
    fcmla z26.s, p0/m, z0.s, z1.s, #\rot
    fcmla z27.s, p0/m, z0.s, z1.s, #\rot
    fcmla z28.s, p0/m, z0.s, z1.s, #\rot
    fcmla z29.s, p0/m, z0.s, z1.s, #\rot
    fcmla z30.s, p0/m, z0.s, z1.s, #\rot
    fcmla z31.s, p0/m, z0.s, z1.s, #\rot
    bne .\label
.endm

.macro SVE_FCMLA_F64_LOOP rot, label
.\label:
    fcmla z8.d , p0/m, z0.d, z1.d, #\rot
    fcmla z9.d , p0/m, z0.d, z1.d, #\rot
    fcmla z10.d, p0/m, z0.d, z1.d, #\rot
    fcmla z11.d, p0/m, z0.d, z1.d, #\rot
    fcmla z12.d, p0/m, z0.d, z1.d, #\rot
    fcmla z13.d, p0/m, z0.d, z1.d, #\rot
    fcmla z14.d, p0/m, z0.d, z1.d, #\rot
    fcmla z15.d, p0/m, z0.d, z1.d, #\rot
    fcmla z16.d, p0/m, z0.d, z1.d, #\rot
    fcmla z17.d, p0/m, z0.d, z1.d, #\rot
    fcmla z18.d, p0/m, z0.d, z1.d, #\rot
    fcmla z19.d, p0/m, z0.d, z1.d, #\rot
    subs x0, x0, #1
    fcmla z20.d, p0/m, z0.d, z1.d, #\rot
    fcmla z21.d, p0/m, z0.d, z1.d, #\rot
    fcmla z22.d, p0/m, z0.d, z1.d, #\rot
    fcmla z23.d, p0/m, z0.d, z1.d, #\rot
    fcmla z24.d, p0/m, z0.d, z1.d, #\rot
    fcmla z25.d, p0/m, z0.d, z1.d, #\rot
    fcmla z26.d, p0/m, z0.d, z1.d, #\rot
    fcmla z27.d, p0/m, z0.d, z1.d, #\rot
    fcmla z28.d, p0/m, z0.d, z1.d, #\rot
    fcmla z29.d, p0/m, z0.d, z1.d, #\rot
    fcmla z30.d, p0/m, z0.d, z1.d, #\rot
    fcmla z31.d, p0/m, z0.d, z1.d, #\rot
    bne .\label
.endm

############
    PROLOGUE sve_fcmla_vv_f32f32f32_0
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LOOP 0, sve_fcmla_vv_f32_0_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f32f32f32_90
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LOOP 90, sve_fcmla_vv_f32_90_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f32f32f32_180
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LOOP 180, sve_fcmla_vv_f32_180_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f32f32f32_270
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LOOP 270, sve_fcmla_vv_f32_270_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_0
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LOOP 0, sve_fcmla_vv_f64_0_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_90
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LOOP 90, sve_fcmla_vv_f64_90_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_180
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LOOP 180, sve_fcmla_vv_f64_180_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_270
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LOOP 270, sve_fcmla_vv_f64_270_L1
    restore_caller_vec
    ret

############
// SVE strict-order reduction: fadda sd, pg, sd, zn   (each lane folded in
// numerical order into scalar destination -> serial dependency chain).
// 24 instructions, all writing to the same scalar -> latency-bound.
    PROLOGUE sve_fadda_v_f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    // GNU as rejects `fmov s8, #0.0` (VFP imm8 can't encode 0); use NEON
    // eor to zero the bottom 128 bits of v8/z8, which covers s8.
    eor v8.16b, v8.16b, v8.16b
.sve_fadda_v_f32_L1:
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    subs x0, x0, #1
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    fadda s8, p0, s8, z0.s
    bne .sve_fadda_v_f32_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_fadda_v_f64
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    // SVE_INIT runs under ptrue.d here, so only every-other .s lane of z8
    // gets the fmov-zero; clear bottom 128 b explicitly so d8 is deterministic.
    eor v8.16b, v8.16b, v8.16b
.sve_fadda_v_f64_L1:
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    subs x0, x0, #1
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    fadda d8, p0, d8, z0.d
    bne .sve_fadda_v_f64_L1
    restore_caller_vec
    ret

############
// SVE any-order tree reduction: faddv sd, pg, zn. 24 distinct destinations
// give independent issue. Throughput-bound, contrast with fadda above.
    PROLOGUE sve_faddv_v_f32
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
.sve_faddv_v_f32_L1:
    faddv s8 , p0, z0.s
    faddv s9 , p0, z0.s
    faddv s10, p0, z0.s
    faddv s11, p0, z0.s
    faddv s12, p0, z0.s
    faddv s13, p0, z0.s
    faddv s14, p0, z0.s
    faddv s15, p0, z0.s
    faddv s16, p0, z0.s
    faddv s17, p0, z0.s
    faddv s18, p0, z0.s
    faddv s19, p0, z0.s
    subs x0, x0, #1
    faddv s20, p0, z0.s
    faddv s21, p0, z0.s
    faddv s22, p0, z0.s
    faddv s23, p0, z0.s
    faddv s24, p0, z0.s
    faddv s25, p0, z0.s
    faddv s26, p0, z0.s
    faddv s27, p0, z0.s
    faddv s28, p0, z0.s
    faddv s29, p0, z0.s
    faddv s30, p0, z0.s
    faddv s31, p0, z0.s
    bne .sve_faddv_v_f32_L1
    restore_caller_vec
    ret

############
    PROLOGUE sve_faddv_v_f64
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
.sve_faddv_v_f64_L1:
    faddv d8 , p0, z0.d
    faddv d9 , p0, z0.d
    faddv d10, p0, z0.d
    faddv d11, p0, z0.d
    faddv d12, p0, z0.d
    faddv d13, p0, z0.d
    faddv d14, p0, z0.d
    faddv d15, p0, z0.d
    faddv d16, p0, z0.d
    faddv d17, p0, z0.d
    faddv d18, p0, z0.d
    faddv d19, p0, z0.d
    subs x0, x0, #1
    faddv d20, p0, z0.d
    faddv d21, p0, z0.d
    faddv d22, p0, z0.d
    faddv d23, p0, z0.d
    faddv d24, p0, z0.d
    faddv d25, p0, z0.d
    faddv d26, p0, z0.d
    faddv d27, p0, z0.d
    faddv d28, p0, z0.d
    faddv d29, p0, z0.d
    faddv d30, p0, z0.d
    faddv d31, p0, z0.d
    bne .sve_faddv_v_f64_L1
    restore_caller_vec
    ret

############
// SVE FCMLA latency variants: all 24 fcmla write to z8 (Pattern A).
.macro SVE_FCMLA_F32_LAT_LOOP rot, label
.\label:
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    subs x0, x0, #1
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    fcmla z8.s, p0/m, z0.s, z1.s, #\rot
    bne .\label
.endm

.macro SVE_FCMLA_F64_LAT_LOOP rot, label
.\label:
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    subs x0, x0, #1
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    fcmla z8.d, p0/m, z0.d, z1.d, #\rot
    bne .\label
.endm

############
    PROLOGUE sve_fcmla_vv_f32f32f32_0_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LAT_LOOP 0, sve_fcmla_vv_f32_0_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f32f32f32_90_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LAT_LOOP 90, sve_fcmla_vv_f32_90_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f32f32f32_180_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LAT_LOOP 180, sve_fcmla_vv_f32_180_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f32f32f32_270_latency
    preserve_caller_vec
    ptrue p0.s
    SVE_INIT
    SVE_FCMLA_F32_LAT_LOOP 270, sve_fcmla_vv_f32_270_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_0_latency
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LAT_LOOP 0, sve_fcmla_vv_f64_0_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_90_latency
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LAT_LOOP 90, sve_fcmla_vv_f64_90_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_180_latency
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LAT_LOOP 180, sve_fcmla_vv_f64_180_lat_L1
    restore_caller_vec
    ret
############
    PROLOGUE sve_fcmla_vv_f64f64f64_270_latency
    preserve_caller_vec
    ptrue p0.d
    SVE_INIT
    SVE_FCMLA_F64_LAT_LOOP 270, sve_fcmla_vv_f64_270_lat_L1
    restore_caller_vec
    ret