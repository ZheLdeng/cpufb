.globl avx512_ifma_madd52_u64
.globl avx512_ifma_madd52_u64_latency

.macro IFMA_16
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        vpmadd52luq %zmm\r, %zmm\r, %zmm\r
    .endr
.endm

avx512_ifma_madd52_u64:
    vzeroall
.Lifma:
    IFMA_16
    dec %rdi
    jne .Lifma
    vzeroupper
    ret

avx512_ifma_madd52_u64_latency:
    vpxorq %zmm0, %zmm0, %zmm0
.Lifma_latency:
    .rept 16
        vpmadd52luq %zmm0, %zmm0, %zmm0
    .endr
    dec %rdi
    jne .Lifma_latency
    vzeroupper
    ret
