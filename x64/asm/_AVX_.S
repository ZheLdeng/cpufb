.globl avx_add_mul_f32f32_f32
.globl avx_add_mul_f64f64_f64
.globl avx_add_mul_f32f32_f32_latency
.globl avx_add_mul_f64f64_f64_latency
.globl avx_add_f32
.globl avx_add_f32_latency
.globl avx_mul_f32
.globl avx_mul_f32_latency
.globl avx_add_f64
.globl avx_add_f64_latency
.globl avx_mul_f64
.globl avx_mul_f64_latency

avx_add_mul_f32f32_f32:
    vxorps %ymm0, %ymm0, %ymm0
    vxorps %ymm1, %ymm1, %ymm1
    vxorps %ymm2, %ymm2, %ymm2
    vxorps %ymm3, %ymm3, %ymm3
    vxorps %ymm4, %ymm4, %ymm4
    vxorps %ymm5, %ymm5, %ymm5
    vxorps %ymm6, %ymm6, %ymm6
    vxorps %ymm7, %ymm7, %ymm7
    vxorps %ymm8, %ymm8, %ymm8
    vxorps %ymm9, %ymm9, %ymm9
    vxorps %ymm10, %ymm10, %ymm10
    vxorps %ymm11, %ymm11, %ymm11
    vxorps %ymm12, %ymm12, %ymm12
    vxorps %ymm13, %ymm13, %ymm13
    vxorps %ymm14, %ymm14, %ymm14
    vxorps %ymm15, %ymm15, %ymm15
.avx.add.mul.f32f32.f32.L1:
    vmulps %ymm0, %ymm0, %ymm0
    vaddps %ymm1, %ymm1, %ymm1
    vmulps %ymm2, %ymm2, %ymm2
    vaddps %ymm3, %ymm3, %ymm3
    vmulps %ymm4, %ymm4, %ymm4
    vaddps %ymm5, %ymm5, %ymm5
    vmulps %ymm6, %ymm6, %ymm6
    vaddps %ymm7, %ymm7, %ymm7
    vmulps %ymm8, %ymm8, %ymm8
    vaddps %ymm9, %ymm9, %ymm9
    vmulps %ymm10, %ymm10, %ymm10
    vaddps %ymm11, %ymm11, %ymm11
    vmulps %ymm12, %ymm12, %ymm12
    vaddps %ymm13, %ymm13, %ymm13
    vmulps %ymm14, %ymm14, %ymm14
    vaddps %ymm15, %ymm15, %ymm15
    sub $0x1, %rdi
    jne .avx.add.mul.f32f32.f32.L1
    ret

avx_add_mul_f64f64_f64:
    vxorpd %ymm0, %ymm0, %ymm0
    vxorpd %ymm1, %ymm1, %ymm1
    vxorpd %ymm2, %ymm2, %ymm2
    vxorpd %ymm3, %ymm3, %ymm3
    vxorpd %ymm4, %ymm4, %ymm4
    vxorpd %ymm5, %ymm5, %ymm5
    vxorpd %ymm6, %ymm6, %ymm6
    vxorpd %ymm7, %ymm7, %ymm7
    vxorpd %ymm8, %ymm8, %ymm8
    vxorpd %ymm9, %ymm9, %ymm9
    vxorpd %ymm10, %ymm10, %ymm10
    vxorpd %ymm11, %ymm11, %ymm11
    vxorpd %ymm12, %ymm12, %ymm12
    vxorpd %ymm13, %ymm13, %ymm13
    vxorpd %ymm14, %ymm14, %ymm14
    vxorpd %ymm15, %ymm15, %ymm15
.avx.add.mul.f64f64.f64.L1:
    vmulpd %ymm0, %ymm0, %ymm0
    vaddpd %ymm1, %ymm1, %ymm1
    vmulpd %ymm2, %ymm2, %ymm2
    vaddpd %ymm3, %ymm3, %ymm3
    vmulpd %ymm4, %ymm4, %ymm4
    vaddpd %ymm5, %ymm5, %ymm5
    vmulpd %ymm6, %ymm6, %ymm6
    vaddpd %ymm7, %ymm7, %ymm7
    vmulpd %ymm8, %ymm8, %ymm8
    vaddpd %ymm9, %ymm9, %ymm9
    vmulpd %ymm10, %ymm10, %ymm10
    vaddpd %ymm11, %ymm11, %ymm11
    vmulpd %ymm12, %ymm12, %ymm12
    vaddpd %ymm13, %ymm13, %ymm13
    vmulpd %ymm14, %ymm14, %ymm14
    vaddpd %ymm15, %ymm15, %ymm15
    sub $0x1, %rdi
    jne .avx.add.mul.f64f64.f64.L1
    ret

avx_add_mul_f32f32_f32_latency:
    vxorps %ymm0, %ymm0, %ymm0
.avx.add.mul.f32f32.f32.latency.L1:
    .rept 8
        vmulps %ymm0, %ymm0, %ymm0
        vaddps %ymm0, %ymm0, %ymm0
    .endr
    dec %rdi
    jne .avx.add.mul.f32f32.f32.latency.L1
    vzeroupper
    ret

avx_add_mul_f64f64_f64_latency:
    vxorpd %ymm0, %ymm0, %ymm0
.avx.add.mul.f64f64.f64.latency.L1:
    .rept 8
        vmulpd %ymm0, %ymm0, %ymm0
        vaddpd %ymm0, %ymm0, %ymm0
    .endr
    dec %rdi
    jne .avx.add.mul.f64f64.f64.latency.L1
    vzeroupper
    ret

.macro AVX16 op
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        \op %ymm\r, %ymm\r, %ymm\r
    .endr
.endm

avx_add_f32:
    vzeroall
.Lavx_add_f32:
    AVX16 vaddps
    dec %rdi
    jne .Lavx_add_f32
    vzeroupper
    ret
avx_add_f32_latency:
    vxorps %ymm0,%ymm0,%ymm0
.Lavx_add_f32_lat:
    .rept 16
        vaddps %ymm0,%ymm0,%ymm0
    .endr
    dec %rdi
    jne .Lavx_add_f32_lat
    vzeroupper
    ret
avx_mul_f32:
    vzeroall
.Lavx_mul_f32:
    AVX16 vmulps
    dec %rdi
    jne .Lavx_mul_f32
    vzeroupper
    ret
avx_mul_f32_latency:
    vxorps %ymm0,%ymm0,%ymm0
.Lavx_mul_f32_lat:
    .rept 16
        vmulps %ymm0,%ymm0,%ymm0
    .endr
    dec %rdi
    jne .Lavx_mul_f32_lat
    vzeroupper
    ret
avx_add_f64:
    vzeroall
.Lavx_add_f64:
    AVX16 vaddpd
    dec %rdi
    jne .Lavx_add_f64
    vzeroupper
    ret
avx_add_f64_latency:
    vxorpd %ymm0,%ymm0,%ymm0
.Lavx_add_f64_lat:
    .rept 16
        vaddpd %ymm0,%ymm0,%ymm0
    .endr
    dec %rdi
    jne .Lavx_add_f64_lat
    vzeroupper
    ret
avx_mul_f64:
    vzeroall
.Lavx_mul_f64:
    AVX16 vmulpd
    dec %rdi
    jne .Lavx_mul_f64
    vzeroupper
    ret
avx_mul_f64_latency:
    vxorpd %ymm0,%ymm0,%ymm0
.Lavx_mul_f64_lat:
    .rept 16
        vmulpd %ymm0,%ymm0,%ymm0
    .endr
    dec %rdi
    jne .Lavx_mul_f64_lat
    vzeroupper
    ret
