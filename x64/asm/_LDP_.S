.globl load_movups_kernel
.globl load_vmovups_kernel
.globl load_movss_stream_kernel
.globl load_movups_xmm_kernel
.globl load_vmovups_zmm_kernel

load_movups_kernel:
    mov %rsi, %r8
    mov %rdi, %r9
.load.movups.kernel.L1:
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
    jne .load.movups.kernel.L1
    vzeroupper
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
    vzeroupper
    ret

load_movss_stream_kernel:
    mov %rdx, %r10
.Lload_movss_outer:
    mov %rdi, %r8
    mov %rsi, %r9
.Lload_movss_inner:
    .set off,0
    .rept 128
        movss off(%r8), %xmm0
        .set off,off+4
    .endr
    add $512, %r8
    dec %r9
    jne .Lload_movss_inner
    dec %r10
    jne .Lload_movss_outer
    ret

load_movups_xmm_kernel:
    mov %rdx, %r10
.Lload_xmm_outer:
    mov %rdi, %r8
    mov %rsi, %r9
.Lload_xmm_inner:
    .set off,0
    .rept 32
        movups off(%r8), %xmm0
        .set off,off+16
    .endr
    add $512, %r8
    dec %r9
    jne .Lload_xmm_inner
    dec %r10
    jne .Lload_xmm_outer
    ret

load_vmovups_zmm_kernel:
    mov %rdx, %r10
.Lload_zmm_outer:
    mov %rdi, %r8
    mov %rsi, %r9
.Lload_zmm_inner:
    vmovups 0(%r8), %zmm0
    vmovups 64(%r8), %zmm1
    vmovups 128(%r8), %zmm2
    vmovups 192(%r8), %zmm3
    vmovups 256(%r8), %zmm4
    vmovups 320(%r8), %zmm5
    vmovups 384(%r8), %zmm6
    vmovups 448(%r8), %zmm7
    add $512, %r8
    dec %r9
    jne .Lload_zmm_inner
    dec %r10
    jne .Lload_zmm_outer
    vzeroupper
    ret

# Store counterparts of the load kernels above, for the write-bandwidth rows:
# (data, blocks of 512 B per pass, passes).  The register contents are
# whatever the caller left; only the addresses matter.
.macro STORE_STREAM name, insn, reg, width, step
.globl \name
\name:
    mov %rdx, %r10
1:
    mov %rdi, %r8
    mov %rsi, %r9
2:
    .set off, 0
    .rept 512 / \step
        \insn %\reg, off(%r8)
        .set off, off + \step
    .endr
    add $512, %r8
    dec %r9
    jne 2b
    dec %r10
    jne 1b
    .if \width >= 32
    vzeroupper
    .endif
    sfence
    ret
.endm

STORE_STREAM store_movups_xmm_kernel, movups, xmm0, 16, 16
STORE_STREAM store_vmovups_kernel, vmovups, ymm0, 32, 32
STORE_STREAM store_vmovups_zmm_kernel, vmovups, zmm0, 64, 64
STORE_STREAM store_movntps_xmm_kernel, movntps, xmm0, 16, 16
STORE_STREAM store_vmovntps_kernel, vmovntps, ymm0, 32, 32
STORE_STREAM store_vmovntps_zmm_kernel, vmovntps, zmm0, 64, 64
# One 16 B store per 64 B line (see the write-allocate verdict).
STORE_STREAM store_partial_line_kernel, movups, xmm0, 16, 64
