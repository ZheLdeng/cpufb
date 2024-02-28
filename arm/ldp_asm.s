.align 4
.global _load_ldp_kernel

_load_ldp_kernel:

    ldp q0, q1,[x0], #32
    ldp q2, q3,[x0], #32
    ldp q4, q5,[x0], #32
    ldp q6, q7,[x0], #32
    ldp q8, q9,[x0], #32

    ldp q10, q11,[x0], #32
    ldp q12, q13,[x0], #32
    ldp q14, q15,[x0], #32
    ldp q16, q17,[x0], #32
    ldp q18, q19,[x0], #32

    ldp q20, q21,[x0], #32
    ldp q22, q23,[x0], #32
    ldp q24, q25,[x0], #32
    ldp q26, q27,[x0], #32
    ldp q28, q29,[x0], #32

    ldp q30, q31,[x0], #32

    ret
