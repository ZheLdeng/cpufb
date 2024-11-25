.global multiple_issue

multiple_issue:
    xorps %xmm0, %xmm0
    xorps %xmm1, %xmm1
    xorps %xmm2, %xmm2
    xorps %xmm3, %xmm3
    xorps %xmm4, %xmm4
    xorps %xmm5, %xmm5

    mov %rdx, %r10
.multiple.issue.L2:
    mov %rdi, %r8
    mov %rsi, %r9
.multiple.issue.L1:

    vmovups (%r8), %ymm0
    imul %r11d, %r11d
    mulps %xmm8, %xmm8
    vmovups (%r8), %ymm1
    imul %r12d, %r12d
    mulps %xmm9, %xmm9
    vmovups (%r8), %ymm2
    imul %r13d, %r13d
    mulps %xmm10, %xmm10
    vmovups (%r8), %ymm3
    imul %r14d, %r14d
    mulps %xmm11, %xmm11

    sub $0x1, %r9
    jne .multiple.issue.L1
    
    sub $0x1, %r10
    jne .multiple.issue.L2
    ret



.globl load_movaps_kernel
.globl load_vmovaps_kernel

load_movaps_kernel:
    mov %rsi, %r8
    mov %rdi, %r9
.load.movaps.kernel.L1:
    vmovups (%r9), %ymm0
    vmovups (%r9), %ymm1
    vmovups (%r9), %ymm2
    vmovups (%r9), %ymm3
    vmovups (%r9), %ymm4
    vmovups (%r9), %ymm5
    vmovups (%r9), %ymm6
    vmovups (%r9), %ymm7
    sub $0x1, %r8
    vmovups (%r9), %ymm8
    vmovups (%r9), %ymm9
    vmovups (%r9), %ymm10
    vmovups (%r9), %ymm11
    vmovups (%r9), %ymm12
    vmovups (%r9), %ymm13
    vmovups (%r9), %ymm14
    vmovups (%r9), %ymm15
    jne .load.movaps.kernel.L1
    ret

load_vmovaps_kernel:
    mov %rdx, %r10
.load.vmovaps.kernel.L2:
    mov %rdi, %r8
    mov %rsi, %r9
.load.vmovaps.kernel.L1:
    vmovups 0(%r8), %ymm0
    vmovups 32(%r8), %ymm1
    vmovups 64(%r8), %ymm2
    vmovups 96(%r8), %ymm3
    vmovups 128(%r8), %ymm4
    vmovups 160(%r8), %ymm5
    vmovups 192(%r8), %ymm6
    vmovups 224(%r8), %ymm7
    vmovups 256(%r8), %ymm8
    vmovups 288(%r8), %ymm9
    vmovups 320(%r8), %ymm10
    vmovups 352(%r8), %ymm11
    vmovups 384(%r8), %ymm12
    vmovups 416(%r8), %ymm13
    vmovups 448(%r8), %ymm14
    vmovups 480(%r8), %ymm15
    add $512, %r8

    sub $0x1, %r9
    jne .load.vmovaps.kernel.L1

    sub $0x1, %r10
    jne .load.vmovaps.kernel.L2
    ret

    movups (%r8), %xmm1
    imul %r12d, %r12d
    mulps %xmm9, %xmm9
    movups (%r8), %xmm2
    imul %r13d, %r13d
    mulps %xmm10, %xmm10
    movups (%r8), %xmm3
    imul %r14d, %r14d
    mulps %xmm11, %xmm11