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

#ifdef _SVE_
#void void sve_multiple_issue(float*,int, int64_t);
    PROLOGUE sve_multiple_issue
    ptrue p0.s
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:

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

    subs x3, x3, #1
    bne 1b

    subs x4, x4, #1
    bne 2b
    ret
#endif
