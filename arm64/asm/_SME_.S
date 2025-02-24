.align 5
#include "common_arm64.h"
.macro SME_INIT
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

    ptrue p0.s
    ptrue p1.s
    mov w8, #0
.endm

############
    PROLOGUE sme_bfmopa_vv_f32bf16bf16
    preserve_caller_vec
    smstart
    SME_INIT
.sme_bfmopa_vv_f32bf16bf16L1:
    BFMOPA za0.S, p0/M, p1/M, z8.H, z8.H
    BFMOPA za1.S, p0/M, p1/M, z9.H, z9.H
    BFMOPA za2.S, p0/M, p1/M, z10.H, z10.H
    BFMOPA za3.S, p0/M, p1/M, z11.H, z11.H
    BFMOPA za0.S, p0/M, p1/M, z12.H, z12.H
    BFMOPA za1.S, p0/M, p1/M, z13.H, z13.H
    BFMOPA za2.S, p0/M, p1/M, z14.H, z14.H
    BFMOPA za3.S, p0/M, p1/M, z15.H, z15.H
    BFMOPA za0.S, p0/M, p1/M, z16.H, z16.H
    BFMOPA za1.S, p0/M, p1/M, z17.H, z17.H
    BFMOPA za2.S, p0/M, p1/M, z18.H, z18.H
    BFMOPA za3.S, p0/M, p1/M, z19.H, z19.H
    subs x0, x0, #1
    BFMOPA za0.S, p0/M, p1/M, z20.H, z20.H
    BFMOPA za1.S, p0/M, p1/M, z21.H, z21.H
    BFMOPA za2.S, p0/M, p1/M, z22.H, z22.H
    BFMOPA za3.S, p0/M, p1/M, z23.H, z23.H
    BFMOPA za0.S, p0/M, p1/M, z24.H, z24.H
    BFMOPA za1.S, p0/M, p1/M, z25.H, z25.H
    BFMOPA za2.S, p0/M, p1/M, z26.H, z26.H
    BFMOPA za3.S, p0/M, p1/M, z27.H, z27.H
    BFMOPA za0.S, p0/M, p1/M, z28.H, z28.H
    BFMOPA za1.S, p0/M, p1/M, z29.H, z29.H
    BFMOPA za2.S, p0/M, p1/M, z30.H, z30.H
    BFMOPA za3.S, p0/M, p1/M, z31.H, z31.H
    bne .sme_bfmopa_vv_f32bf16bf16L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme_fmopa_vv_f32f32f32
    preserve_caller_vec
    smstart
    SME_INIT
.sme_fmopa_vv_f32f32f32L1:
    FMOPA za0.S, p0/M, p1/M, z8.S, z9.S
    FMOPA za1.S, p0/M, p1/M, z10.S, z11.S
    FMOPA za2.S, p0/M, p1/M, z12.S, z13.S
    FMOPA za3.S, p0/M, p1/M, z14.S, z15.S
    FMOPA za0.S, p0/M, p1/M, z16.S, z17.S
    FMOPA za1.S, p0/M, p1/M, z18.S, z19.S
    FMOPA za2.S, p0/M, p1/M, z20.S, z21.S
    FMOPA za3.S, p0/M, p1/M, z22.S, z23.S
    FMOPA za0.S, p0/M, p1/M, z24.S, z25.S
    FMOPA za1.S, p0/M, p1/M, z26.S, z27.S
    FMOPA za2.S, p0/M, p1/M, z28.S, z29.S
    FMOPA za3.S, p0/M, p1/M, z30.S, z31.S
    subs x0, x0, #1
    FMOPA za0.S, p0/M, p1/M, z8.S, z9.S
    FMOPA za1.S, p0/M, p1/M, z10.S, z11.S
    FMOPA za2.S, p0/M, p1/M, z12.S, z13.S
    FMOPA za3.S, p0/M, p1/M, z14.S, z15.S
    FMOPA za0.S, p0/M, p1/M, z16.S, z17.S
    FMOPA za1.S, p0/M, p1/M, z18.S, z19.S
    FMOPA za2.S, p0/M, p1/M, z20.S, z21.S
    FMOPA za3.S, p0/M, p1/M, z22.S, z23.S
    FMOPA za0.S, p0/M, p1/M, z24.S, z25.S
    FMOPA za1.S, p0/M, p1/M, z26.S, z27.S
    FMOPA za2.S, p0/M, p1/M, z28.S, z29.S
    FMOPA za3.S, p0/M, p1/M, z30.S, z31.S
    bne .sme_fmopa_vv_f32f32f32L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme_fmopa_vv_f32f16f16
    preserve_caller_vec
    smstart
    SME_INIT
.sme_fmopa_vv_f32f16f16L1:
    FMOPA za0.S, p0/M, p1/M, z8.H, z9.H
    FMOPA za1.S, p0/M, p1/M, z10.H, z11.H
    FMOPA za2.S, p0/M, p1/M, z12.H, z13.H
    FMOPA za3.S, p0/M, p1/M, z14.H, z15.H
    FMOPA za0.S, p0/M, p1/M, z16.H, z17.H
    FMOPA za1.S, p0/M, p1/M, z18.H, z19.H
    FMOPA za2.S, p0/M, p1/M, z20.H, z21.H
    FMOPA za3.S, p0/M, p1/M, z22.H, z23.H
    FMOPA za0.S, p0/M, p1/M, z24.H, z25.H
    FMOPA za1.S, p0/M, p1/M, z26.H, z27.H
    FMOPA za2.S, p0/M, p1/M, z28.H, z29.H
    FMOPA za3.S, p0/M, p1/M, z30.H, z31.H
    subs x0, x0, #1
    FMOPA za0.S, p0/M, p1/M, z8.H, z9.H
    FMOPA za1.S, p0/M, p1/M, z10.H, z11.H
    FMOPA za2.S, p0/M, p1/M, z12.H, z13.H
    FMOPA za3.S, p0/M, p1/M, z14.H, z15.H
    FMOPA za0.S, p0/M, p1/M, z16.H, z17.H
    FMOPA za1.S, p0/M, p1/M, z18.H, z19.H
    FMOPA za2.S, p0/M, p1/M, z20.H, z21.H
    FMOPA za3.S, p0/M, p1/M, z22.H, z23.H
    FMOPA za0.S, p0/M, p1/M, z24.H, z25.H
    FMOPA za1.S, p0/M, p1/M, z26.H, z27.H
    FMOPA za2.S, p0/M, p1/M, z28.H, z29.H
    FMOPA za3.S, p0/M, p1/M, z30.H, z31.H
    bne .sme_fmopa_vv_f32f16f16L1
    smstop
    restore_caller_vec
    ret
############
    PROLOGUE sme_smopa_vv_i32i8i8
    preserve_caller_vec
    smstart
    SME_INIT
.sme_smopa_vv_i32i8i8L1:
    SMOPA za0.S, p0/M, p1/M, z8.B, z9.B
    SMOPA za1.S, p0/M, p1/M, z10.B, z11.B
    SMOPA za2.S, p0/M, p1/M, z12.B, z13.B
    SMOPA za3.S, p0/M, p1/M, z14.B, z15.B
    SMOPA za0.S, p0/M, p1/M, z16.B, z17.B
    SMOPA za1.S, p0/M, p1/M, z18.B, z19.B
    SMOPA za2.S, p0/M, p1/M, z20.B, z21.B
    SMOPA za3.S, p0/M, p1/M, z22.B, z23.B
    SMOPA za0.S, p0/M, p1/M, z24.B, z25.B
    SMOPA za1.S, p0/M, p1/M, z26.B, z27.B
    SMOPA za2.S, p0/M, p1/M, z28.B, z29.B
    SMOPA za3.S, p0/M, p1/M, z30.B, z31.B
    subs x0, x0, #1
    SMOPA za0.S, p0/M, p1/M, z8.B, z9.B
    SMOPA za1.S, p0/M, p1/M, z10.B, z11.B
    SMOPA za2.S, p0/M, p1/M, z12.B, z13.B
    SMOPA za3.S, p0/M, p1/M, z14.B, z15.B
    SMOPA za0.S, p0/M, p1/M, z16.B, z17.B
    SMOPA za1.S, p0/M, p1/M, z18.B, z19.B
    SMOPA za2.S, p0/M, p1/M, z20.B, z21.B
    SMOPA za3.S, p0/M, p1/M, z22.B, z23.B
    SMOPA za0.S, p0/M, p1/M, z24.B, z25.B
    SMOPA za1.S, p0/M, p1/M, z26.B, z27.B
    SMOPA za2.S, p0/M, p1/M, z28.B, z29.B
    SMOPA za3.S, p0/M, p1/M, z30.B, z31.B
    bne .sme_smopa_vv_i32i8i8L1
    restore_caller_vec
    smstop
    ret
############
    PROLOGUE sme_ldr_kernel
    preserve_caller_vec
    RDSVL x6, #1
    mov w12, #0
    mov w13, #1
    mov w14, #2
    mov w15, #3
    mov x4, x2
    smstart
    2:
    mov x3, x1
    mov x5, x0
    1:
    ldr za[w12, #0], [x5]
    ldr za[w12, #1], [x5, #1, MUL VL]
    ldr za[w12, #2], [x5, #2, MUL VL]
    ldr za[w12, #3], [x5, #3, MUL VL]
    ldr za[w12, #4], [x5, #4, MUL VL]
    ldr za[w12, #5], [x5, #5, MUL VL]
    ldr za[w12, #6], [x5, #6, MUL VL]
    ldr za[w12, #7], [x5, #7, MUL VL]
    ldr za[w12, #8], [x5, #8, MUL VL]
    ldr za[w12, #9], [x5, #9, MUL VL]
    ldr za[w12, #10], [x5, #10, MUL VL]
    ldr za[w12, #11], [x5, #11, MUL VL]
    ldr za[w12, #12], [x5, #12, MUL VL]
    ldr za[w12, #13], [x5, #13, MUL VL]
    ldr za[w12, #14], [x5, #14, MUL VL]
    ldr za[w12, #15], [x5, #15, MUL VL]
    add x5, x5, x6, lsl #4
    
    subs x3, x3, x6, lsl #2 
    bne 1b

    subs x4, x4, #1
    bne 2b
    smstop
    restore_caller_vec
    ret

    
############
    PROLOGUE sme_ld1wV_kernel
    //LD1W { <ZAt><HV>.S[<Ws>, <offs>] }, <Pg>/Z, [<Xn|SP>{, <Xm>, LSL #2}]
    //(cache_data, inner_loop, looptime)
    preserve_caller_vec
    RDSVL x6, #1
    mov w12, #0
    mov w13, #1
    mov w14, #2
    mov w15, #3
    mov x7, #16
    add x18, x7, x7
    add x19, x7, x7, lsl #1
    add x20, x12, x18, lsl #1
    add x21, x7, x7, lsl #2
    add x22, x18, x7, lsl #2
    add x23, x19, x7, lsl #2 
    mov x4, x2
    smstart
    2:
    mov x3, x1
    mov x5, x0
    mov x24, x0 
    1: 
    add x24, x5, x6, lsl #3
    LD1w ZA0V.S[W12, #0], P0/Z, [x5]
    LD1w ZA1V.S[W12, #0], P0/Z, [x5, x7, LSL #2]
    LD1w ZA2V.S[W12, #0], P0/Z, [x5, x18, LSL #2]
    LD1w ZA3V.S[W12, #0], P0/Z, [x5, x19, LSL #2]
    LD1w ZA0V.S[W12, #1], P0/Z, [x5, x20, LSL #2]
    LD1w ZA1V.S[W12, #1], P0/Z, [x5, x21, LSL #2]
    LD1w ZA2V.S[W12, #1], P0/Z, [x5, x22, LSL #2]
    LD1w ZA3V.S[W12, #1], P0/Z, [x5, x23, LSL #2]
    add x5, x5, x6, lsl #4
    LD1w ZA0V.S[W12, #0], P0/Z, [x24]
    LD1w ZA1V.S[W12, #0], P0/Z, [x24, x7, LSL #2]
    LD1w ZA2V.S[W12, #0], P0/Z, [x24, x18, LSL #2]
    LD1w ZA3V.S[W12, #0], P0/Z, [x24, x19, LSL #2]
    LD1w ZA0V.S[W12, #1], P0/Z, [x24, x20, LSL #2]
    LD1w ZA1V.S[W12, #1], P0/Z, [x24, x21, LSL #2]
    LD1w ZA2V.S[W12, #1], P0/Z, [x24, x22, LSL #2]
    LD1w ZA3V.S[W12, #1], P0/Z, [x24, x23, LSL #2]
    
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




    subs x3, x3, x6, lsl #2
    bne 1b

    subs x4, x4, #1
    bne 2b

    smstop
    restore_caller_vec
    ret
############
    PROLOGUE sme_ld1wH_kernel
    //LD1W { <ZAt><HV>.S[<Ws>, <offs>] }, <Pg>/Z, [<Xn|SP>{, <Xm>, LSL #2}]
    //(cache_data, inner_loop, looptime)
    preserve_caller_vec
    RDSVL x6, #1
    mov w12, #0
    mov w13, #1
    mov w14, #2
    mov w15, #3
    mov x7, #16
    add x18, x7, x7
    add x19, x7, x7, lsl #1
    add x20, x12, x18, lsl #1
    add x21, x7, x7, lsl #2
    add x22, x18, x7, lsl #2
    add x23, x19, x7, lsl #2
    mov x4, x2
    smstart
    2:
    mov x3, x1
    mov x5, x0
    mov x24, x0
    1:
    add x24, x5, x6, lsl #3
    LD1w ZA0H.S[W12, #0], P0/Z, [x5]
    LD1w ZA1H.S[W12, #0], P0/Z, [x5, x7, LSL #2]
    LD1w ZA2H.S[W12, #0], P0/Z, [x5, x18, LSL #2]
    LD1w ZA3H.S[W12, #0], P0/Z, [x5, x19, LSL #2]
    LD1w ZA0H.S[W12, #1], P0/Z, [x5, x20, LSL #2]
    LD1w ZA1H.S[W12, #1], P0/Z, [x5, x21, LSL #2]
    LD1w ZA2H.S[W12, #1], P0/Z, [x5, x22, LSL #2]
    LD1w ZA3H.S[W12, #1], P0/Z, [x5, x23, LSL #2]
    add x5, x5, x6, lsl #4
    LD1w ZA0H.S[W12, #0], P0/Z, [x24]
    LD1w ZA1H.S[W12, #0], P0/Z, [x24, x7, LSL #2]
    LD1w ZA2H.S[W12, #0], P0/Z, [x24, x18, LSL #2]
    LD1w ZA3H.S[W12, #0], P0/Z, [x24, x19, LSL #2]
    LD1w ZA0H.S[W12, #1], P0/Z, [x24, x20, LSL #2]
    LD1w ZA1H.S[W12, #1], P0/Z, [x24, x21, LSL #2]
    LD1w ZA2H.S[W12, #1], P0/Z, [x24, x22, LSL #2]
    LD1w ZA3H.S[W12, #1], P0/Z, [x24, x23, LSL #2]
    
    subs x3, x3, x6, lsl #2
    bne 1b

    subs x4, x4, #1
    bne 2b

    smstop
    restore_caller_vec
    ret
##########
#void void sme_multiple_issue(float*,int, int64_t);
    PROLOGUE sme_multiple_issue

    RDSVL x6, #1
    mov w12, #0
    mov w13, #1
    mov w14, #2
    mov w15, #3
    mov x4, x2
    smstart
    ptrue p0.S
    ptrue p1.S
    2:
    mov x3, x1
    mov x5, x0


    1:
    ldr za[w12, #0], [x5]
    ldr za[w12, #1], [x5, #1, MUL VL]
    ldr za[w12, #2], [x5, #2, MUL VL]
    ldr za[w12, #3], [x5, #3, MUL VL]
    ldr za[w12, #4], [x5, #4, MUL VL]
    ldr za[w12, #5], [x5, #5, MUL VL]
    ldr za[w12, #6], [x5, #6, MUL VL]
    ldr za[w12, #7], [x5, #7, MUL VL]
    ldr za[w12, #8], [x5, #8, MUL VL]
    ldr za[w12, #9], [x5, #9, MUL VL]
    ldr za[w12, #10], [x5, #10, MUL VL]
    ldr za[w12, #11], [x5, #11, MUL VL]
    ldr za[w12, #12], [x5, #12, MUL VL]
    ldr za[w12, #13], [x5, #13, MUL VL]
    ldr za[w12, #14], [x5, #14, MUL VL]
    ldr za[w12, #15], [x5, #15, MUL VL]
    
    FMOPA za0.S, p0/M, p1/M, z8.S, z9.S
    FMOPA za1.S, p0/M, p1/M, z10.S, z11.S
    FMOPA za2.S, p0/M, p1/M, z12.S, z13.S
    FMOPA za3.S, p0/M, p1/M, z14.S, z15.S
    FMOPA za0.S, p0/M, p1/M, z16.S, z17.S
    FMOPA za1.S, p0/M, p1/M, z18.S, z19.S
    FMOPA za2.S, p0/M, p1/M, z20.S, z21.S
    FMOPA za3.S, p0/M, p1/M, z22.S, z23.S
    FMOPA za0.S, p0/M, p1/M, z24.S, z25.S
    FMOPA za1.S, p0/M, p1/M, z26.S, z27.S
    FMOPA za2.S, p0/M, p1/M, z28.S, z29.S
    FMOPA za3.S, p0/M, p1/M, z30.S, z31.S

    FMOPA za0.S, p0/M, p1/M, z8.S, z9.S
    FMOPA za1.S, p0/M, p1/M, z10.S, z11.S
    FMOPA za2.S, p0/M, p1/M, z12.S, z13.S
    FMOPA za3.S, p0/M, p1/M, z14.S, z15.S
    FMOPA za0.S, p0/M, p1/M, z16.S, z17.S
    FMOPA za1.S, p0/M, p1/M, z18.S, z19.S
    FMOPA za2.S, p0/M, p1/M, z20.S, z21.S
    FMOPA za3.S, p0/M, p1/M, z22.S, z23.S
    FMOPA za0.S, p0/M, p1/M, z24.S, z25.S
    FMOPA za1.S, p0/M, p1/M, z26.S, z27.S
    FMOPA za2.S, p0/M, p1/M, z28.S, z29.S
    FMOPA za3.S, p0/M, p1/M, z30.S, z31.S

    subs x3, x3, #1
    bne 1b    

    subs x4, x4, #1
    bne 2b
    smstop
    ret