.align 5

#ifdef __APPLE__
.global _load_ldp_kernel

_load_ldp_kernel:
#else 
.global load_ldp_kernel

load_ldp_kernel:    
#endif
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:
    ldp q0, q1,[x5], #32
    ldp q2, q3,[x5], #32
    ldp q4, q5,[x5], #32
    ldp q6, q7,[x5], #32
    ldp q8, q9,[x5], #32
    ldp q10, q11,[x5], #32
    ldp q12, q13,[x5], #32
    ldp q14, q15,[x5], #32
    ldp q16, q17,[x5], #32
    ldp q18, q19,[x5], #32
    ldp q20, q21,[x5], #32
    ldp q22, q23,[x5], #32
    ldp q24, q25,[x5], #32
    ldp q26, q27,[x5], #32
    ldp q28, q29,[x5], #32
    ldp q30, q31,[x5], #32

    subs x3, x3, #1
    bne 1b    

    subs x4, x4, #1
    bne 2b
    ret
