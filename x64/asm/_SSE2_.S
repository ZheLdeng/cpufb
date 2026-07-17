.globl sse2_add_mul_f64f64_f64
.globl sse2_add_f64
.globl sse2_add_f64_latency
.globl sse2_mul_f64
.globl sse2_mul_f64_latency

sse2_add_mul_f64f64_f64:
    xorpd %xmm0, %xmm0
    xorpd %xmm1, %xmm1
    xorpd %xmm2, %xmm2
    xorpd %xmm3, %xmm3
    xorpd %xmm4, %xmm4
    xorpd %xmm5, %xmm5
    xorpd %xmm6, %xmm6
    xorpd %xmm7, %xmm7
    xorpd %xmm8, %xmm8
    xorpd %xmm9, %xmm9
    xorpd %xmm10, %xmm10
    xorpd %xmm11, %xmm11
    xorpd %xmm12, %xmm12
    xorpd %xmm13, %xmm13
    xorpd %xmm14, %xmm14
    xorpd %xmm15, %xmm15
.sse2.add.mul.f64f64.f64.L1:
    mulpd %xmm0, %xmm0
    addpd %xmm1, %xmm1
    mulpd %xmm2, %xmm2
    addpd %xmm3, %xmm3
    mulpd %xmm4, %xmm4
    addpd %xmm5, %xmm5
    mulpd %xmm6, %xmm6
    addpd %xmm7, %xmm7
    sub $0x1, %rdi
    mulpd %xmm8, %xmm8
    addpd %xmm9, %xmm9
    mulpd %xmm10, %xmm10
    addpd %xmm11, %xmm11
    mulpd %xmm12, %xmm12
    addpd %xmm13, %xmm13
    mulpd %xmm14, %xmm14
    addpd %xmm15, %xmm15
    jne .sse2.add.mul.f64f64.f64.L1
    ret

sse2_add_f64:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        xorpd %xmm\r,%xmm\r
    .endr
.Lsse2_add:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        addpd %xmm\r,%xmm\r
    .endr
    dec %rdi
    jne .Lsse2_add
    ret
sse2_add_f64_latency:
    xorpd %xmm0,%xmm0
.Lsse2_add_lat:
    .rept 16
        addpd %xmm0,%xmm0
    .endr
    dec %rdi
    jne .Lsse2_add_lat
    ret
sse2_mul_f64:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        xorpd %xmm\r,%xmm\r
    .endr
.Lsse2_mul:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        mulpd %xmm\r,%xmm\r
    .endr
    dec %rdi
    jne .Lsse2_mul
    ret
sse2_mul_f64_latency:
    xorpd %xmm0,%xmm0
.Lsse2_mul_lat:
    .rept 16
        mulpd %xmm0,%xmm0
    .endr
    dec %rdi
    jne .Lsse2_mul_lat
    ret
