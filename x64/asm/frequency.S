.text

# These kernels deliberately use only the SSE/SSE2 baseline guaranteed by the
# x86-64 ABI.  Each hot loop contains 16 benchmark instructions; loop control
# is not included in the reported instruction rate.

.p2align 4
.globl cpufb_x64_frequency_fsu32
.type cpufb_x64_frequency_fsu32, @function
cpufb_x64_frequency_fsu32:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        xorps %xmm\r, %xmm\r
    .endr
.Lcpufb_frequency_fsu32:
    .irp r,0,1,2,3,4,5,6,7
        addps %xmm\r, %xmm\r
    .endr
    dec %rdi
    .irp r,8,9,10,11,12,13,14,15
        addps %xmm\r, %xmm\r
    .endr
    jne .Lcpufb_frequency_fsu32
    ret
.size cpufb_x64_frequency_fsu32, .-cpufb_x64_frequency_fsu32

.p2align 4
.globl cpufb_x64_frequency_fsu64
.type cpufb_x64_frequency_fsu64, @function
cpufb_x64_frequency_fsu64:
    .irp r,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        xorpd %xmm\r, %xmm\r
    .endr
.Lcpufb_frequency_fsu64:
    .irp r,0,1,2,3,4,5,6,7
        addpd %xmm\r, %xmm\r
    .endr
    dec %rdi
    .irp r,8,9,10,11,12,13,14,15
        addpd %xmm\r, %xmm\r
    .endr
    jne .Lcpufb_frequency_fsu64
    ret
.size cpufb_x64_frequency_fsu64, .-cpufb_x64_frequency_fsu64

.p2align 4
.globl cpufb_x64_frequency_load
.type cpufb_x64_frequency_load, @function
cpufb_x64_frequency_load:
.Lcpufb_frequency_load:
    .irp r,0,1,2,3,4,5,6,7
        movups (%rdi), %xmm\r
    .endr
    dec %rsi
    .irp r,8,9,10,11,12,13,14,15
        movups (%rdi), %xmm\r
    .endr
    jne .Lcpufb_frequency_load
    ret
.size cpufb_x64_frequency_load, .-cpufb_x64_frequency_load


# Core-clock calibration for hosts without PMU access: a register-register
# ADD dependency chain retires exactly one ADD per core cycle on every x86-64
# microarchitecture, so 16 x loop_time / elapsed is the running core clock.
# The addend comes from a register because recent Intel cores fold small
# immediate adds at rename.  Loop control is off the chain.
.p2align 4
.globl cpufb_x64_frequency_add_chain
.type cpufb_x64_frequency_add_chain, @function
cpufb_x64_frequency_add_chain:
    xor %eax, %eax
.Lcpufb_frequency_add_chain:
    .rept 16
        add %rsi, %rax
    .endr
    dec %rdi
    jne .Lcpufb_frequency_add_chain
    ret
.size cpufb_x64_frequency_add_chain, .-cpufb_x64_frequency_add_chain

.section .note.GNU-stack,"",@progbits
