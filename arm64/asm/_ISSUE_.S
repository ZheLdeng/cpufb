.align 4
#ifdef __APPLE__
.global _multiple_issue

_multiple_issue:
#else 
.global multiple_issue

multiple_issue:    
#endif
    
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:

    ldr q0, [x5]
    ldr q1, [x5]
    ldr q2, [x5]
    ldr q3, [x5]
    ldr q4, [x5]
    ldr q5, [x5]
    ldr q6, [x5]
    ldr q7, [x5]

    mul w8, w8, w8
    mul w9, w9, w9
    mul w10,w10,w10
    mul w11,w11,w11
    mul w12, w12, w12
    mul w13, w13, w13
    mul w14, w14, w14
    mul w15, w15, w15

    fmla v16.4s, v16.4s, v16.4s
    fmla v17.4s, v17.4s, v17.4s
    fmla v18.4s, v18.4s, v18.4s
    fmla v19.4s, v19.4s, v19.4s
    fmla v20.4s, v20.4s, v20.4s
    fmla v21.4s, v21.4s, v21.4s
    fmla v22.4s, v22.4s, v22.4s
    fmla v23.4s, v23.4s, v23.4s

    
    

    subs x3, x3, #1
    bne 1b    

    subs x4, x4, #1
    bne 2b
    ret
