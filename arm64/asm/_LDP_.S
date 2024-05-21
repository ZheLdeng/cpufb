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

    ldp q0, q1,[x5]
    ldp q2, q3,[x5]
    ldp q4, q5,[x5]
    ldp q6, q7,[x5]
    ldp q8, q9,[x5]
    ldp q10, q11,[x5]
    ldp q12, q13,[x5]
    ldp q14, q15,[x5]
    ldp q16, q17,[x5]
    ldp q18, q19,[x5]
    ldp q20, q21,[x5]
    ldp q22, q23,[x5]
    ldp q24, q25,[x5]
    ldp q26, q27,[x5]
    ldp q28, q29,[x5]
    ldp q30, q31,[x5]
    add x5, x5, #512
    
    subs x3, x3, #1
    bne 1b    

    subs x4, x4, #1
    bne 2b
    ret


#ifdef __APPLE__
.global _load_ldr_kernel

_load_ldr_kernel:
#else 
.global load_ldr_kernel

load_ldr_kernel:    
#endif
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
    //ldr q8, [x5]
    //ldr q9, [x5]
    //ldr q10, [x5]
    //ldr q11, [x5]
    //ldr q12, [x5]
    //ldr q13, [x5]
    //ldr q14, [x5]
    //ldr q15, [x5]
    ldr q16, [x5]
    ldr q17, [x5]
    ldr q18, [x5]
    ldr q19, [x5]
    ldr q20, [x5]
    ldr q21, [x5]
    ldr q22, [x5]
    ldr q23, [x5]
    ldr q24, [x5]
    ldr q25, [x5]
    ldr q26, [x5]
    ldr q27, [x5]
    ldr q28, [x5]
    ldr q29, [x5]
    ldr q30, [x5]
    ldr q31, [x5]
    
    subs x3, x3, #1
    bne 1b    
    ret
    