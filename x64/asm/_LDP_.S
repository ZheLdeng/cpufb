.globl load_movups_kernel
.globl load_vmovups_kernel

load_movups_kernel:
    mov %rsi, %r8
    mov %rdi, %r9
.load.movups.kernel.L1:
    movups (%r9), %xmm0
    movups (%r9), %xmm1
    movups (%r9), %xmm2
    movups (%r9), %xmm3
    movups (%r9), %xmm4
    movups (%r9), %xmm5
    movups (%r9), %xmm6
    movups (%r9), %xmm7
    sub $0x1, %r8
    movups (%r9), %xmm8
    movups (%r9), %xmm9
    movups (%r9), %xmm10
    movups (%r9), %xmm11
    movups (%r9), %xmm12
    movups (%r9), %xmm13
    movups (%r9), %xmm14
    movups (%r9), %xmm15
    jne .load.movups.kernel.L1
    ret

load_vmovups_kernel:
    mov %rdx, %r10
.load.vmovups.kernel.L2:
    mov %rdi, %r8
    mov %rsi, %r9
.load.vmovups.kernel.L1:
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
    jne .load.vmovups.kernel.L1

    sub $0x1, %r10
    jne .load.vmovups.kernel.L2
    ret
