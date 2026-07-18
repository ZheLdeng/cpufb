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

.section .note.GNU-stack,"",@progbits
