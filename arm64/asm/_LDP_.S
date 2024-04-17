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
    mov x29, x0
    1:
    ldp q0, q1,[x29]
    ldp q2, q3,[x29]
    ldp q4, q5,[x29]
    ldp q6, q7,[x29]
    ldp q8, q9,[x29]
    ldp q10, q11,[x29]
    ldp q12, q13,[x29]
    ldp q14, q15,[x29]
    ldp q16, q17,[x29]
    ldp q18, q19,[x29]
    ldp q20, q21,[x29]
    ldp q22, q23,[x29]
    ldp q24, q25,[x29]
    ldp q26, q27,[x29]
    ldp q28, q29,[x29]
    ldp q30, q31,[x29]

    subs x3, x3, #1
    bne 1b    

    subs x4, x4, #1
    bne 2b
    ret
