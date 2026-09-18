.globl sse_add_mul_f32f32_f32
.globl sse_add_f32
.globl sse_add_f32_latency
.globl sse_mul_f32
.globl sse_mul_f32_latency

sse_add_mul_f32f32_f32:
    xorps %xmm0, %xmm0
    xorps %xmm1, %xmm1
    xorps %xmm2, %xmm2
    xorps %xmm3, %xmm3
    xorps %xmm4, %xmm4
    xorps %xmm5, %xmm5
    xorps %xmm6, %xmm6
    xorps %xmm7, %xmm7
    xorps %xmm8, %xmm8
    xorps %xmm9, %xmm9
    xorps %xmm10, %xmm10
    xorps %xmm11, %xmm11
    xorps %xmm12, %xmm12
    xorps %xmm13, %xmm13
    xorps %xmm14, %xmm14
    xorps %xmm15, %xmm15
.sse.add.mul.f32f32_f32.L1:
    mulps %xmm0, %xmm0
    addps %xmm1, %xmm1
    mulps %xmm2, %xmm2
    addps %xmm3, %xmm3
    mulps %xmm4, %xmm4
    addps %xmm5, %xmm5
    mulps %xmm6, %xmm6
    addps %xmm7, %xmm7
    sub $0x1, %rdi
    mulps %xmm8, %xmm8
    addps %xmm9, %xmm9
    mulps %xmm10, %xmm10
    addps %xmm11, %xmm11
    mulps %xmm12, %xmm12
    addps %xmm13, %xmm13
    mulps %xmm14, %xmm14
    addps %xmm15, %xmm15
    jne .sse.add.mul.f32f32_f32.L1
    ret

sse_add_f32:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        xorps %xmm\r,%xmm\r
    .endr
.Lsse_add:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        addps %xmm\r,%xmm\r
    .endr
    dec %rdi
    jne .Lsse_add
    ret
sse_add_f32_latency:
    xorps %xmm0,%xmm0
.Lsse_add_lat:
    .rept 16
        addps %xmm0,%xmm0
    .endr
    dec %rdi
    jne .Lsse_add_lat
    ret
sse_mul_f32:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        xorps %xmm\r,%xmm\r
    .endr
.Lsse_mul:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        mulps %xmm\r,%xmm\r
    .endr
    dec %rdi
    jne .Lsse_mul
    ret
sse_mul_f32_latency:
    xorps %xmm0,%xmm0
.Lsse_mul_lat:
    .rept 16
        mulps %xmm0,%xmm0
    .endr
    dec %rdi
    jne .Lsse_mul_lat
    ret
