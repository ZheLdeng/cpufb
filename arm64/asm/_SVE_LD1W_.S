#include "common_arm64.h"
#void void load_ld1w_kernel(float*, int ,int);
    PROLOGUE load_ld1w_kernel
    mov x11, x2
    ptrue p2.s
outer_loop:
    mov x9, x0
    mov x10, x1
    whilelt p1.b, xzr, x10
inner_loop:
    ld1w z0.s, p2/z, [x9]
    ld1w z1.s, p2/z, [x9, #1, MUL VL]
    ld1w z2.s, p2/z, [x9, #2, MUL VL]
    ld1w z3.s, p2/z, [x9, #3, MUL VL]
    ld1w z4.s, p2/z, [x9, #4, MUL VL]
    ld1w z5.s, p2/z, [x9, #5, MUL VL]
    ld1w z6.s, p2/z, [x9, #6, MUL VL]
    ld1w z7.s, p2/z, [x9, #7, MUL VL]
    incb  x9, POW2, MUL#8
    decb x10, pow2, mul#2
    whilelt p1.s, xzr, x10
    b.first inner_loop

    subs x11, x11, #1
    bne outer_loop
out:
    ret
