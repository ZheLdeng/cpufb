.globl avx2_add_s32
.globl avx2_add_s32_latency
.globl avx2_mul_s32
.globl avx2_mul_s32_latency

.macro AVX2_16 op
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        \op %ymm\r, %ymm\r, %ymm\r
    .endr
.endm

avx2_add_s32:
    vzeroall
.Lavx2_add:
    AVX2_16 vpaddd
    dec %rdi
    jne .Lavx2_add
    vzeroupper
    ret

avx2_add_s32_latency:
    vpxor %ymm0, %ymm0, %ymm0
.Lavx2_add_latency:
    .rept 16
        vpaddd %ymm0, %ymm0, %ymm0
    .endr
    dec %rdi
    jne .Lavx2_add_latency
    vzeroupper
    ret

avx2_mul_s32:
    vzeroall
.Lavx2_mul:
    AVX2_16 vpmulld
    dec %rdi
    jne .Lavx2_mul
    vzeroupper
    ret

avx2_mul_s32_latency:
    vpxor %ymm0, %ymm0, %ymm0
.Lavx2_mul_latency:
    .rept 16
        vpmulld %ymm0, %ymm0, %ymm0
    .endr
    dec %rdi
    jne .Lavx2_mul_latency
    vzeroupper
    ret
