.align 5
#include "common_arm64.h"
#void load_ldp_kernel(float*, int, int64_t);
#void load_ldr_kernel(float*, int64_t);
    PROLOGUE load_ldp_kernel
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:

    ldp q0, q1,[x5, #0]
    ldp q2, q3,[x5, #32]
    ldp q4, q5,[x5, #64]
    ldp q6, q7,[x5, #96]
    ldp q8, q9,[x5, #128]
    ldp q10, q11,[x5, #160]
    ldp q12, q13,[x5, #192]
    ldp q14, q15,[x5, #224]
    ldp q16, q17,[x5, #256]
    ldp q18, q19,[x5, #288]
    ldp q20, q21,[x5, #320]
    ldp q22, q23,[x5, #352]
    ldp q24, q25,[x5, #384]
    ldp q26, q27,[x5, #416]
    ldp q28, q29,[x5, #448]
    ldp q30, q31,[x5, #480]
    add x5, x5, #512

    subs x3, x3, #1
    bne 1b

    subs x4, x4, #1
    bne 2b
    ret

###############################
#void load_neon_ld1b_kernel(float*, int, int64_t);
    PROLOGUE load_neon_ld1b_kernel
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:
    add x6, x5, #64
    add x7, x5, #128
    add x8, x5, #192
    ld1 {v0.16b, v1.16b, v2.16b, v3.16b}, [x5]
    ld1 {v4.16b, v5.16b, v6.16b, v7.16b}, [x6]
    ld1 {v16.16b, v17.16b, v18.16b, v19.16b}, [x7]
    ld1 {v20.16b, v21.16b, v22.16b, v23.16b}, [x8]
    add x5, x5, #256
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

###############################
#void load_neon_ld1h_kernel(float*, int, int64_t);
    PROLOGUE load_neon_ld1h_kernel
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:
    add x6, x5, #64
    add x7, x5, #128
    add x8, x5, #192
    ld1 {v0.8h, v1.8h, v2.8h, v3.8h}, [x5]
    ld1 {v4.8h, v5.8h, v6.8h, v7.8h}, [x6]
    ld1 {v16.8h, v17.8h, v18.8h, v19.8h}, [x7]
    ld1 {v20.8h, v21.8h, v22.8h, v23.8h}, [x8]
    add x5, x5, #256
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

###############################
#void load_neon_ld1w_kernel(float*, int, int64_t);
    PROLOGUE load_neon_ld1w_kernel
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:
    add x6, x5, #64
    add x7, x5, #128
    add x8, x5, #192
    ld1 {v0.4s, v1.4s, v2.4s, v3.4s}, [x5]
    ld1 {v4.4s, v5.4s, v6.4s, v7.4s}, [x6]
    ld1 {v16.4s, v17.4s, v18.4s, v19.4s}, [x7]
    ld1 {v20.4s, v21.4s, v22.4s, v23.4s}, [x8]
    add x5, x5, #256
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

###############################
#void load_neon_ld1d_kernel(float*, int, int64_t);
    PROLOGUE load_neon_ld1d_kernel
    mov x4, x2
    2:
    mov x3, x1
    mov x5, x0
    1:
    add x6, x5, #64
    add x7, x5, #128
    add x8, x5, #192
    ld1 {v0.2d, v1.2d, v2.2d, v3.2d}, [x5]
    ld1 {v4.2d, v5.2d, v6.2d, v7.2d}, [x6]
    ld1 {v16.2d, v17.2d, v18.2d, v19.2d}, [x7]
    ld1 {v20.2d, v21.2d, v22.2d, v23.2d}, [x8]
    add x5, x5, #256
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    ret

###############################
    PROLOGUE load_ldr_kernel
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
