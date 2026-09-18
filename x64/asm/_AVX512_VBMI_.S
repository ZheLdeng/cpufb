.globl avx512_vbmi_permb_u8
.globl avx512_vbmi_permb_u8_latency

.macro VBMI_16
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        vpermb %zmm\r, %zmm\r, %zmm\r
    .endr
.endm

avx512_vbmi_permb_u8:
    vzeroall
.Lvbmi:
    VBMI_16
    dec %rdi
    jne .Lvbmi
    vzeroupper
    ret

avx512_vbmi_permb_u8_latency:
    vpxorq %zmm0, %zmm0, %zmm0
.Lvbmi_latency:
    .rept 16
        vpermb %zmm0, %zmm0, %zmm0
    .endr
    dec %rdi
    jne .Lvbmi_latency
    vzeroupper
    ret
