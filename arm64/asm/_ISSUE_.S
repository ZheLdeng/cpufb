.align 4
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
