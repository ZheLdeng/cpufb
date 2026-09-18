.globl avx512_vpopcnt_s32
.globl avx512_vpopcnt_s32_latency

.macro VPOPCNT_16
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        vpopcntd %zmm\r, %zmm\r
    .endr
.endm

avx512_vpopcnt_s32:
    vzeroall
.Lvpopcnt:
    VPOPCNT_16
    dec %rdi
    jne .Lvpopcnt
    vzeroupper
    ret

avx512_vpopcnt_s32_latency:
    vpxorq %zmm0, %zmm0, %zmm0
.Lvpopcnt_latency:
    .rept 16
        vpopcntd %zmm0, %zmm0
    .endr
    dec %rdi
    jne .Lvpopcnt_latency
    vzeroupper
    ret
