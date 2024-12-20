.global multiple_issue

multiple_issue:

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


