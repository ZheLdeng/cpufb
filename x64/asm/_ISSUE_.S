.global multiple_issue
.global multiple_issue_avx
.global multiple_issue_avx512

multiple_issue:
    # Zero the accumulators: caller garbage would turn the self-multiply
    # chains into NaN/denormal work with data-dependent timing.
    .irp reg,8,9,10,11,12,13,14,15
        xorps %xmm\reg, %xmm\reg
    .endr
    mov %rdx, %r10
.multiple.issue.L2:
    mov %rdi, %r8
    mov %rsi, %r9
.multiple.issue.L1:

    movups (%r8), %xmm0
    mulps %xmm8, %xmm8
    movups (%r8), %xmm1
    mulps %xmm9, %xmm9
    movups (%r8), %xmm2
    mulps %xmm10, %xmm10
    movups (%r8), %xmm3
    mulps %xmm11, %xmm11
    movups (%r8), %xmm4              
    mulps %xmm12, %xmm12
    movups (%r8), %xmm5
    mulps %xmm13, %xmm13
    movups (%r8), %xmm6
    mulps %xmm14, %xmm14
    movups (%r8), %xmm7
    mulps %xmm15, %xmm15

    movups (%r8), %xmm0
    mulps %xmm8, %xmm8
    movups (%r8), %xmm1
    mulps %xmm9, %xmm9
    movups (%r8), %xmm2
    mulps %xmm10, %xmm10
    movups (%r8), %xmm3
    mulps %xmm11, %xmm11
    movups (%r8), %xmm4              
    mulps %xmm12, %xmm12
    movups (%r8), %xmm5
    mulps %xmm13, %xmm13
    movups (%r8), %xmm6
    mulps %xmm14, %xmm14
    movups (%r8), %xmm7
    mulps %xmm15, %xmm15
                                                                   

    sub $0x1, %r9
    jne .multiple.issue.L1
    
    sub $0x1, %r10
    jne .multiple.issue.L2
    ret

multiple_issue_avx:
    .irp reg,8,9,10,11,12,13,14,15
        vxorps %ymm\reg, %ymm\reg, %ymm\reg
    .endr
    mov %rdx, %r10
.Lissue_avx_outer:
    mov %rdi, %r8
    mov %rsi, %r9
.Lissue_avx_inner:
    .rept 2
        vmovups (%r8), %ymm0
        vfmadd231ps %ymm8, %ymm8, %ymm8
        vmovups 32(%r8), %ymm1
        vfmadd231ps %ymm9, %ymm9, %ymm9
        vmovups 64(%r8), %ymm2
        vfmadd231ps %ymm10, %ymm10, %ymm10
        vmovups 96(%r8), %ymm3
        vfmadd231ps %ymm11, %ymm11, %ymm11
        vmovups 128(%r8), %ymm4
        vfmadd231ps %ymm12, %ymm12, %ymm12
        vmovups 160(%r8), %ymm5
        vfmadd231ps %ymm13, %ymm13, %ymm13
        vmovups 192(%r8), %ymm6
        vfmadd231ps %ymm14, %ymm14, %ymm14
        vmovups 224(%r8), %ymm7
        vfmadd231ps %ymm15, %ymm15, %ymm15
    .endr
    dec %r9
    jne .Lissue_avx_inner
    dec %r10
    jne .Lissue_avx_outer
    vzeroupper
    ret

multiple_issue_avx512:
    .irp reg,8,9,10,11,12,13,14,15
        vpxord %zmm\reg, %zmm\reg, %zmm\reg
    .endr
    mov %rdx, %r10
.Lissue_avx512_outer:
    mov %rdi, %r8
    mov %rsi, %r9
.Lissue_avx512_inner:
    .rept 2
        vmovups (%r8), %zmm0
        vfmadd231ps %zmm8, %zmm8, %zmm8
        vmovups 64(%r8), %zmm1
        vfmadd231ps %zmm9, %zmm9, %zmm9
        vmovups 128(%r8), %zmm2
        vfmadd231ps %zmm10, %zmm10, %zmm10
        vmovups 192(%r8), %zmm3
        vfmadd231ps %zmm11, %zmm11, %zmm11
        vmovups 256(%r8), %zmm4
        vfmadd231ps %zmm12, %zmm12, %zmm12
        vmovups 320(%r8), %zmm5
        vfmadd231ps %zmm13, %zmm13, %zmm13
        vmovups 384(%r8), %zmm6
        vfmadd231ps %zmm14, %zmm14, %zmm14
        vmovups 448(%r8), %zmm7
        vfmadd231ps %zmm15, %zmm15, %zmm15
    .endr
    dec %r9
    jne .Lissue_avx512_inner
    dec %r10
    jne .Lissue_avx512_outer
    vzeroupper
    ret

