/* Issue-probe kernels for x86-64, generated at assembly time from
 * x64/kernel/issue_classes.def.  See common/issue_probe.hpp for the method
 * and common/issue_layout.h for the grid.
 *
 * Every kernel is
 *     void issue_<...>(const void *l1_buffer / rdi /, int64_t loops / rsi /)
 * and runs a fixed straight-line body `loops` times.  r12 holds the ALU
 * addend.  Vector instructions use their own register as every operand, so
 * all sixteen registers 0-15 of the kernel's widest form are chains;
 * general-purpose chains are rax, rcx, rdx, r8-r11, rbx, rbp and r13-r15.
 *
 * Adding a class takes one ISSUE_INS_<name> macro below and one line in the
 * .def file.
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
 * Framework.
 * ---------------------------------------------------------------------- */

.macro ISSUE_CLASS_ATTR name, bank, mode, width
    .set ISSUE_BANK_\name, \bank
    .set ISSUE_MODE_\name, \mode
    .set ISSUE_WIDTH_\name, \width
.endm

/* Chain indices are positions in the bank's list: vector registers 0-15
 * (16), general registers rax, rcx, rdx, r8-r11, rbx, rbp, r13-r15 (12). */
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

.macro ISSUE_SET_SLOT slot, base, chains
    .set ISSUE_BASE_\slot, \base
    .set ISSUE_CHAINS_\slot, \chains
    .set ISSUE_CTR_\slot, 0
.endm

.macro ISSUE_STEP slot, cls
    ISSUE_EMIT \cls, ISSUE_BANK_\cls, ISSUE_BASE_\slot+ISSUE_CTR_\slot
    .set ISSUE_CTR_\slot, ISSUE_CTR_\slot + 1
    .if ISSUE_CTR_\slot >= ISSUE_CHAINS_\slot
        .set ISSUE_CTR_\slot, 0
    .endif
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

/* Two vector classes: registers 0-7 and 8-15, eight chains each, which
 * covers a latency of four on two pipes for either class. */
.macro ISSUE_SPLIT_VEC major, minor
    ISSUE_SET_SLOT \major, 8, 8
    ISSUE_SET_SLOT \minor, 0, 8
.endm

.macro ISSUE_PARTITION2 A, B, a, b
    ISSUE_SLOT_DEFAULT A, \A
    ISSUE_SLOT_DEFAULT B, \B
    .if (ISSUE_BANK_\A == ISSUE_BANK_VEC) && (ISSUE_BANK_\B == ISSUE_BANK_VEC)
        .if \a >= \b
            ISSUE_SPLIT_VEC A, B
        .else
            ISSUE_SPLIT_VEC B, A
        .endif
    .endif
.endm

.macro ISSUE_PARTITION3 A, B, C, a, b, c
    ISSUE_SLOT_DEFAULT A, \A
    ISSUE_SLOT_DEFAULT B, \B
    ISSUE_SLOT_DEFAULT C, \C
    .set issue_vec_a, ISSUE_BANK_\A == ISSUE_BANK_VEC
    .set issue_vec_b, ISSUE_BANK_\B == ISSUE_BANK_VEC
    .set issue_vec_c, ISSUE_BANK_\C == ISSUE_BANK_VEC
    .if issue_vec_a && issue_vec_b && issue_vec_c
        ISSUE_SET_SLOT A, 0, 8
        ISSUE_SET_SLOT B, 8, 4
        ISSUE_SET_SLOT C, 12, 4
    .elseif issue_vec_a && issue_vec_b
        .if \a >= \b
            ISSUE_SPLIT_VEC A, B
        .else
            ISSUE_SPLIT_VEC B, A
        .endif
    .elseif issue_vec_a && issue_vec_c
        .if \a >= \c
            ISSUE_SPLIT_VEC A, C
        .else
            ISSUE_SPLIT_VEC C, A
        .endif
    .elseif issue_vec_b && issue_vec_c
        .if \b >= \c
            ISSUE_SPLIT_VEC B, C
        .else
            ISSUE_SPLIT_VEC C, B
        .endif
    .endif
.endm

/* Function entry: callee-saved registers, the addend, zeroed vector
 * registers of the kernel's widest form so that FMA chains stay finite. */
.macro ISSUE_KERNEL_BEGIN name, width
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
    .if \width == 2
        .irp r, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
            vpxord %zmm\r, %zmm\r, %zmm\r
        .endr
    .elseif \width == 1
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

.macro ISSUE_KERNEL_END width
    dec %rsi
    jne 1b
    .if \width >= 1
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

/* Evenly spread patterns (weighted round robin). */
.macro ISSUE_PATTERN2 A, B, a, b
    .set issue_err_a, \a
    .set issue_err_b, \b
    .rept (\a + \b)
        .if issue_err_a >= issue_err_b
            ISSUE_STEP A, \A
            .set issue_err_a, issue_err_a - (\a + \b)
        .else
            ISSUE_STEP B, \B
            .set issue_err_b, issue_err_b - (\a + \b)
        .endif
        .set issue_err_a, issue_err_a + \a
        .set issue_err_b, issue_err_b + \b
    .endr
.endm

.macro ISSUE_PATTERN3 A, B, C, a, b, c
    .set issue_err_a, \a
    .set issue_err_b, \b
    .set issue_err_c, \c
    .rept (\a + \b + \c)
        .if (issue_err_a >= issue_err_b) && (issue_err_a >= issue_err_c)
            ISSUE_STEP A, \A
            .set issue_err_a, issue_err_a - (\a + \b + \c)
        .elseif issue_err_b >= issue_err_c
            ISSUE_STEP B, \B
            .set issue_err_b, issue_err_b - (\a + \b + \c)
        .else
            ISSUE_STEP C, \C
            .set issue_err_c, issue_err_c - (\a + \b + \c)
        .endif
        .set issue_err_a, issue_err_a + \a
        .set issue_err_b, issue_err_b + \b
        .set issue_err_c, issue_err_c + \c
    .endr
.endm

/* Widest form among the classes of a kernel. */
.macro ISSUE_WIDTH2 A, B
    .if ISSUE_WIDTH_\A >= ISSUE_WIDTH_\B
        .set issue_width, ISSUE_WIDTH_\A
    .else
        .set issue_width, ISSUE_WIDTH_\B
    .endif
.endm

.macro ISSUE_WIDTH3 A, B, C
    ISSUE_WIDTH2 \A, \B
    .if ISSUE_WIDTH_\C > issue_width
        .set issue_width, ISSUE_WIDTH_\C
    .endif
.endm

/* -------------------------------------------------------------------------
 * Kernel shapes.
 * ---------------------------------------------------------------------- */

.macro ISSUE_PURE_KERNEL cls, sfx, chains
    ISSUE_SET_SLOT A, 0, \chains
    ISSUE_KERNEL_BEGIN issue_\cls\()_pure_\sfx, ISSUE_WIDTH_\cls
    .rept ISSUE_PURE_BODY
        ISSUE_STEP A, \cls
    .endr
    ISSUE_KERNEL_END ISSUE_WIDTH_\cls
.endm

.macro ISSUE_PURE_KERNELS cls
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_PURE_KERNEL \cls, h, 8
        ISSUE_PURE_KERNEL \cls, f, 16
    .else
        ISSUE_PURE_KERNEL \cls, h, 6
        ISSUE_PURE_KERNEL \cls, f, 12
    .endif
.endm

.macro ISSUE_PAIR_KERNEL A, B, a, b
    ISSUE_WIDTH2 \A, \B
    ISSUE_PARTITION2 \A, \B, \a, \b
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\a\()x\b\()_il, issue_width
    .rept ISSUE_MIX_REPEATS(\a + \b)
        ISSUE_PATTERN2 \A, \B, \a, \b
    .endr
    ISSUE_KERNEL_END issue_width

    ISSUE_PARTITION2 \A, \B, \a, \b
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\a\()x\b\()_bl, issue_width
    .rept ISSUE_MIX_REPEATS(\a + \b) / 4
        .rept 4 * \a
            ISSUE_STEP A, \A
        .endr
        .rept 4 * \b
            ISSUE_STEP B, \B
        .endr
    .endr
    ISSUE_KERNEL_END issue_width
.endm

.macro ISSUE_TRIPLE_KERNEL A, B, C, a, b, c
    ISSUE_WIDTH3 \A, \B, \C
    ISSUE_PARTITION3 \A, \B, \C, \a, \b, \c
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\C\()_\a\()x\b\()x\c\()_il, issue_width
    .rept ISSUE_MIX_REPEATS(\a + \b + \c)
        ISSUE_PATTERN3 \A, \B, \C, \a, \b, \c
    .endr
    ISSUE_KERNEL_END issue_width
.endm

/* -------------------------------------------------------------------------
 * Expansion of the .def file: attributes, pure kernels, mixes.
 * ---------------------------------------------------------------------- */

#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature) \
    ISSUE_CLASS_ATTR name, ISSUE_BANK_##bank, ISSUE_MODE_##mode, width
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE

#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature) \
    ISSUE_PURE_KERNELS name
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE

#define ISSUE_ASM_PAIR(A, B, a, b) ISSUE_PAIR_KERNEL A, B, a, b
#define ISSUE_ASM_TRIPLE(A, B, C, a, b, c) ISSUE_TRIPLE_KERNEL A, B, C, a, b, c
#define ISSUE_CLASS(name, instruction, unit, bank, mode, width, feature)
#define ISSUE_PAIR(A, B, label) ISSUE_RATIO_GRID(ISSUE_ASM_PAIR, A, B, ;)
#define ISSUE_TRIPLE(A, B, C, label) ISSUE_TRIPLE_GRID(ISSUE_ASM_TRIPLE, A, B, C, ;)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
#undef ISSUE_ASM_PAIR
#undef ISSUE_ASM_TRIPLE

.section .note.GNU-stack,"",@progbits
