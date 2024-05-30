
.global load_ld1w_kernel

load_ld1w_kernel:
    mov x27, x2
    ptrue p2.s
outer_loop:
    mov x29, x0
    mov x28, x1
    whilelt p1.b, xzr, x28
inner_loop:
    ld1w z0.s, p2/z, [x29]
    ld1w z1.s, p2/z, [x29, #1, MUL VL]
    ld1w z2.s, p2/z, [x29, #2, MUL VL]
    ld1w z3.s, p2/z, [x29, #3, MUL VL]
    //ld1w z4.s, p2/z, [x29, #4, MUL VL]
    //ld1w z5.s, p2/z, [x29, #5, MUL VL]
    //ld1w z6.s, p2/z, [x29, #6, MUL VL]
    //ld1w z7.s, p2/z, [x29, #7, MUL VL]

    incb  x29, POW2, MUL#4
    decb x28
    whilelt p1.s, xzr, x28
    b.first inner_loop

    subs x27, x27, #1
    bne outer_loop

out:
    ret