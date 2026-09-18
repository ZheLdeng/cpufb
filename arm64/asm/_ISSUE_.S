#include "common_arm64.h"
#void void multiple_issue(float*,int, int64_t);
    PROLOGUE multiple_issue
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:

    ldr q0, [x5]
    mul w8, w8, w8
    fmla v16.4s, v16.4s, v16.4s
    ldr q1, [x5]
    mul w9, w9, w9
    fmla v17.4s, v17.4s, v17.4s
    ldr q2, [x5]
    mul w10,w10,w10
    fmla v18.4s, v18.4s, v18.4s
    ldr q3, [x5]
    mul w11,w11,w11
    fmla v19.4s, v19.4s, v19.4s
    ldr q4, [x5]
    mul w12, w12, w12
    fmla v20.4s, v20.4s, v20.4s
    ldr q5, [x5]
    mul w13, w13, w13
    fmla v21.4s, v21.4s, v21.4s
    ldr q6, [x5]
    mul w14, w14, w14
    fmla v22.4s, v22.4s, v22.4s
    ldr q7, [x5]
    mul w15, w15, w15
    fmla v23.4s, v23.4s, v23.4s
    
    ldr q0, [x5]
    mul w8, w8, w8
    fmla v16.4s, v16.4s, v16.4s
    ldr q1, [x5]
    mul w9, w9, w9
    fmla v17.4s, v17.4s, v17.4s
    ldr q2, [x5]
    mul w10,w10,w10
    fmla v18.4s, v18.4s, v18.4s
    ldr q3, [x5]
    mul w11,w11,w11
    fmla v19.4s, v19.4s, v19.4s
    ldr q4, [x5]
    mul w12, w12, w12
    fmla v20.4s, v20.4s, v20.4s
    ldr q5, [x5]
    mul w13, w13, w13
    fmla v21.4s, v21.4s, v21.4s
    ldr q6, [x5]
    mul w14, w14, w14
    fmla v22.4s, v22.4s, v22.4s
    ldr q7, [x5]
    mul w15, w15, w15
    fmla v23.4s, v23.4s, v23.4s


    subs x3, x3, #1
    bne 1b    

    subs x4, x4, #1
    bne 2b
    ret

############
#void void neon_multiple_issue(float*,int, int64_t);
    PROLOGUE neon_multiple_issue
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:

    // Match the SVE 1:1 mixed-issue test with 16 independent FMLA chains.
    ldr q0, [x5]
    fmla v16.4s, v16.4s, v16.4s
    ldr q1, [x5]
    fmla v17.4s, v17.4s, v17.4s
    ldr q2, [x5]
    fmla v18.4s, v18.4s, v18.4s
    ldr q3, [x5]
    fmla v19.4s, v19.4s, v19.4s
    ldr q4, [x5]
    fmla v20.4s, v20.4s, v20.4s
    ldr q5, [x5]
    fmla v21.4s, v21.4s, v21.4s
    ldr q6, [x5]
    fmla v22.4s, v22.4s, v22.4s
    ldr q7, [x5]
    fmla v23.4s, v23.4s, v23.4s

    ldr q0, [x5]
    fmla v24.4s, v24.4s, v24.4s
    ldr q1, [x5]
    fmla v25.4s, v25.4s, v25.4s
    ldr q2, [x5]
    fmla v26.4s, v26.4s, v26.4s
    ldr q3, [x5]
    fmla v27.4s, v27.4s, v27.4s
    ldr q4, [x5]
    fmla v28.4s, v28.4s, v28.4s
    ldr q5, [x5]
    fmla v29.4s, v29.4s, v29.4s
    ldr q6, [x5]
    fmla v30.4s, v30.4s, v30.4s
    ldr q7, [x5]
    fmla v31.4s, v31.4s, v31.4s

    subs x3, x3, #1
    bne 1b

    subs x4, x4, #1
    bne 2b
    ret

#ifdef _SVE_
#void void sve_multiple_issue(float*,int, int64_t);
    PROLOGUE sve_multiple_issue
    ptrue p0.s
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:

    // Keep 16 independent FMLA chains so the four-cycle accumulator latency
    // does not cap the mixed benchmark at two FMLA instructions per cycle.
    ld1w z0.s, p0/z, [x5]
    fmla z16.s, p0/m, z16.s, z16.s
    ld1w z1.s, p0/z, [x5]
    fmla z17.s, p0/m, z17.s, z17.s
    ld1w z2.s, p0/z, [x5]
    fmla z18.s, p0/m, z18.s, z18.s
    ld1w z3.s, p0/z, [x5]
    fmla z19.s, p0/m, z19.s, z19.s
    ld1w z4.s, p0/z, [x5]
    fmla z20.s, p0/m, z20.s, z20.s
    ld1w z5.s, p0/z, [x5]
    fmla z21.s, p0/m, z21.s, z21.s
    ld1w z6.s, p0/z, [x5]
    fmla z22.s, p0/m, z22.s, z22.s
    ld1w z7.s, p0/z, [x5]
    fmla z23.s, p0/m, z23.s, z23.s

    ld1w z0.s, p0/z, [x5]
    fmla z24.s, p0/m, z24.s, z24.s
    ld1w z1.s, p0/z, [x5]
    fmla z25.s, p0/m, z25.s, z25.s
    ld1w z2.s, p0/z, [x5]
    fmla z26.s, p0/m, z26.s, z26.s
    ld1w z3.s, p0/z, [x5]
    fmla z27.s, p0/m, z27.s, z27.s
    ld1w z4.s, p0/z, [x5]
    fmla z28.s, p0/m, z28.s, z28.s
    ld1w z5.s, p0/z, [x5]
    fmla z29.s, p0/m, z29.s, z29.s
    ld1w z6.s, p0/z, [x5]
    fmla z30.s, p0/m, z30.s, z30.s
    ld1w z7.s, p0/z, [x5]
    fmla z31.s, p0/m, z31.s, z31.s

    subs x3, x3, #1
    bne 1b

    subs x4, x4, #1
    bne 2b
    ret

.macro SCALAR_ADDS count
    add x6, x6, #1
    .if \count >= 2
    add x7, x7, #1
    .endif
    .if \count >= 3
    add x8, x8, #1
    .endif
    .if \count >= 4
    add x9, x9, #1
    .endif
    .if \count >= 5
    add x10, x10, #1
    .endif
    .if \count >= 6
    add x11, x11, #1
    .endif
.endm

.macro SVE_SCALAR_ADD_BODY add_count
    // Ten load/FMLA pairs provide ten independent four-cycle FMLA chains.
    // Four evenly spaced scalar blocks make the exact SVE:add ratio 5:N.
    ld1w z0.s, p0/z, [x5]
    fmla z16.s, p0/m, z16.s, z16.s
    ld1w z1.s, p0/z, [x5]
    fmla z17.s, p0/m, z17.s, z17.s
    SCALAR_ADDS \add_count

    ld1w z2.s, p0/z, [x5]
    fmla z18.s, p0/m, z18.s, z18.s
    ld1w z3.s, p0/z, [x5]
    fmla z19.s, p0/m, z19.s, z19.s
    ld1w z4.s, p0/z, [x5]
    fmla z20.s, p0/m, z20.s, z20.s
    SCALAR_ADDS \add_count

    ld1w z5.s, p0/z, [x5]
    fmla z21.s, p0/m, z21.s, z21.s
    ld1w z6.s, p0/z, [x5]
    fmla z22.s, p0/m, z22.s, z22.s
    SCALAR_ADDS \add_count

    ld1w z7.s, p0/z, [x5]
    fmla z23.s, p0/m, z23.s, z23.s
    ld1w z0.s, p0/z, [x5]
    fmla z24.s, p0/m, z24.s, z24.s
    ld1w z1.s, p0/z, [x5]
    fmla z25.s, p0/m, z25.s, z25.s
    SCALAR_ADDS \add_count
.endm

.macro DEFINE_SVE_SCALAR_ADD name, add_count
    PROLOGUE \name
    ptrue p0.s
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:
    SVE_SCALAR_ADD_BODY \add_count
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret
.endm

    DEFINE_SVE_SCALAR_ADD sve_scalar_add_5_1, 1
    DEFINE_SVE_SCALAR_ADD sve_scalar_add_5_2, 2
    DEFINE_SVE_SCALAR_ADD sve_scalar_add_5_3, 3
    DEFINE_SVE_SCALAR_ADD sve_scalar_add_5_4, 4
    DEFINE_SVE_SCALAR_ADD sve_scalar_add_5_5, 5
    DEFINE_SVE_SCALAR_ADD sve_scalar_add_5_6, 6
#endif
