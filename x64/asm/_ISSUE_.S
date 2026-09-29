/* Issue-probe kernels for x86-64.  The framework (slots, partitions,
 * patterns, kernel shapes, the .def expansion) is common/issue_asm.h; this
 * file supplies what is specific to the architecture: the instruction of
 * each class, the register banks, the function entry and exit.
 *
 * Every kernel is
 *     void issue_<...>(const void *l1_buffer / rdi /, int64_t loops / rsi /)
 * r12 holds the ALU addend.  Vector instructions use their own register as
 * every operand, so all sixteen registers 0-15 of the kernel's widest form
 * are chains; general-purpose chains are rax, rcx, rdx, r8-r11, rbx, rbp and
 * r13-r15.  The class attribute (6th .def field) is the register width the
 * kernel must zero at entry: 0 xmm, 1 ymm, 2 zmm.
 *
 * Adding a class takes one ISSUE_INS_<name> macro here and one line in
 * x64/kernel/issue_classes.def.
 */

#include "issue_layout.h"

    .text

/* -------------------------------------------------------------------------
 * Instruction classes: one instruction on register \r.
 * ---------------------------------------------------------------------- */

.macro ISSUE_INS_alu r
    add %r12, %\r
.endm

.macro ISSUE_INS_sse_add r
    addps %xmm\r, %xmm\r
.endm

.macro ISSUE_INS_sse_load r
    movups (\r * 16)(%rdi), %xmm\r
.endm

.macro ISSUE_INS_avx_fma r
    vfmadd231ps %ymm\r, %ymm\r, %ymm\r
.endm

.macro ISSUE_INS_avx_load r
    vmovups (\r * 32)(%rdi), %ymm\r
.endm

.macro ISSUE_INS_avx512_fma r
    vfmadd231ps %zmm\r, %zmm\r, %zmm\r
.endm

.macro ISSUE_INS_avx512_load r
    vmovups (\r * 64)(%rdi), %zmm\r
.endm

/* -------------------------------------------------------------------------
 * Register banks.  Chain indices are positions in a bank's list: vector
 * registers 0-15 (16), general registers rax, rcx, rdx, r8-r11, rbx, rbp,
 * r13-r15 (12).
 * ---------------------------------------------------------------------- */

.macro ISSUE_EMIT cls, bank, idx
    .if \bank == ISSUE_BANK_VEC
        .irp r, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
            .if (\idx) == \r
                ISSUE_INS_\cls \r
            .endif
        .endr
    .else
        .set issue_position, 0
        .irp r, rax,rcx,rdx,r8,r9,r10,r11,rbx,rbp,r13,r14,r15
            .if (\idx) == issue_position
                ISSUE_INS_\cls \r
            .endif
            .set issue_position, issue_position + 1
        .endr
    .endif
.endm

    .set ISSUE_HALF_VEC, 8
    .set ISSUE_FULL_VEC, 16
    .set ISSUE_HALF_GPR, 6
    .set ISSUE_FULL_GPR, 12
    .set ISSUE_HALF_ZA, 1
    .set ISSUE_FULL_ZA, 1

.macro ISSUE_SLOT_PURE slot, cls, chains
    ISSUE_SET_SLOT \slot, 0, \chains
.endm

/* A vector class alone in its bank rotates over all sixteen registers; a
 * general-purpose class over eight of its twelve (a multiple of the pattern
 * repeat count, so the rotation restarts at the loop boundary). */
.macro ISSUE_SLOT_DEFAULT slot, cls
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_SET_SLOT \slot, 0, 16
    .else
        ISSUE_SET_SLOT \slot, 0, 8
    .endif
.endm

/* Two vector classes: registers 8-15 and 0-7, eight chains each, which
 * covers a latency of four on two pipes for either class. */
.macro ISSUE_SPLIT_VEC major, minor
    ISSUE_SET_SLOT \major, 8, 8
    ISSUE_SET_SLOT \minor, 0, 8
.endm

.macro ISSUE_SPLIT_VEC3
    ISSUE_SET_SLOT A, 0, 8
    ISSUE_SET_SLOT B, 8, 4
    ISSUE_SET_SLOT C, 12, 4
.endm

/* The kernel attribute is the widest form among its classes. */
.macro ISSUE_COMBINE_ATTR2 A, B
    .if ISSUE_ATTR_\A >= ISSUE_ATTR_\B
        .set issue_attr, ISSUE_ATTR_\A
    .else
        .set issue_attr, ISSUE_ATTR_\B
    .endif
.endm

.macro ISSUE_COMBINE_ATTR3 A, B, C
    ISSUE_COMBINE_ATTR2 \A, \B
    .if ISSUE_ATTR_\C > issue_attr
        .set issue_attr, ISSUE_ATTR_\C
    .endif
.endm

/* -------------------------------------------------------------------------
 * Function entry and exit: callee-saved registers, the addend, zeroed
 * vector registers of the kernel's widest form so that FMA chains stay
 * finite, vzeroupper on the way out.
 * ---------------------------------------------------------------------- */

.macro ISSUE_KERNEL_BEGIN name, mode, attr
    .p2align 5
    .globl \name
    .type \name, @function
\name:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    mov $3, %r12
    .if \attr == 2
        .irp r, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
            vpxord %zmm\r, %zmm\r, %zmm\r
        .endr
    .elseif \attr == 1
        .irp r, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
            vxorps %ymm\r, %ymm\r, %ymm\r
        .endr
    .else
        .irp r, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
            xorps %xmm\r, %xmm\r
        .endr
    .endif
1:
.endm

.macro ISSUE_KERNEL_END mode, attr
    dec %rsi
    jne 1b
    .if \attr >= 1
        vzeroupper
    .endif
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
.endm

#include "issue_asm.h"

.section .note.GNU-stack,"",@progbits
