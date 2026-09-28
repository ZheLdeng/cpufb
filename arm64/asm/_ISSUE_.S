// Issue-probe kernels, generated at assembly time from
// arm64/kernel/issue_classes.def.  See common/issue_probe.hpp for how the
// probe reads them and common/issue_layout.h for the grid.
//
// Every kernel is
//     void issue_<...>(const void *l1_buffer /* x0 */, int64_t loops /* x1 */)
// and runs a fixed straight-line body `loops` times.  A class contributes one
// instruction per step, rotating over its own chain of registers, so no two
// instructions of the body depend on each other except through that
// rotation.  x2 holds the ALU addend; v0/v1 (z0/z1) are constant operands.
//
// Adding a class takes one ISSUE_INS_<name> macro below and one line in the
// .def file.  The macro receives the register number of the current chain.

#include "common_arm64.h"
#include "issue_layout.h"

    .text

// ---------------------------------------------------------------------------
// Instruction classes: one instruction on register \r.
// ---------------------------------------------------------------------------

.macro ISSUE_INS_alu r
    add x\r, x\r, x2
.endm

.macro ISSUE_INS_neon_fmla r
    fmla v\r\().4s, v0.4s, v1.4s
.endm

// Sixteen consecutive lines of the buffer; the offset follows the register.
.macro ISSUE_INS_neon_ldr r
    ldr q\r, [x0, #((\r - 8) & 15) * 16]
.endm

#ifdef _SVE_
.macro ISSUE_INS_sve_fmla r
    fmla z\r\().s, p0/m, z0.s, z1.s
.endm

.macro ISSUE_INS_sve_ld1w r
    ld1w z\r\().s, p0/z, [x0, #((\r - 8) & 7), mul vl]
.endm
#endif

#ifdef _SME_
// Streaming-mode classes: the kernel enters streaming mode around the body.
.macro ISSUE_INS_sme_fmopa r
    fmopa za\r\().s, p0/m, p1/m, z0.s, z1.s
.endm

.macro ISSUE_INS_ssve_fmla r
    fmla z\r\().s, p0/m, z0.s, z1.s
.endm

.macro ISSUE_INS_ssve_ld1w r
    ld1w z\r\().s, p0/z, [x0, #((\r - 8) & 7), mul vl]
.endm
#endif

// ---------------------------------------------------------------------------
// Framework.
// ---------------------------------------------------------------------------

// Zeroes one Z register.  A helper because inside a .irp nested in a macro
// the assembler only substitutes \r when a comma or line end follows it.
.macro ISSUE_ZERO_Z r
    dup z\r\().s, #0
.endm

// Class attributes as assembler symbols.
.macro ISSUE_CLASS_ATTR name, bank, mode, sve
    .set ISSUE_BANK_\name, \bank
    .set ISSUE_MODE_\name, \mode
    .set ISSUE_SVE_\name, \sve
.endm

// Emits one instruction of class \cls on chain \idx of its bank.  Chain
// indices are positions in the bank's register list: vectors v8-v31 (24),
// general registers x9-x15 and x19-x27 (16), ZA tiles 0-3 (4).
.macro ISSUE_EMIT cls, bank, idx
    .if \bank == ISSUE_BANK_VEC
        .irp r, 8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
            .if (\idx) == \r
                ISSUE_INS_\cls \r
            .endif
        .endr
    .elseif \bank == ISSUE_BANK_GPR
        .set issue_position, 0
        .irp r, 9,10,11,12,13,14,15,19,20,21,22,23,24,25,26,27
            .if (\idx) == issue_position
                ISSUE_INS_\cls \r
            .endif
            .set issue_position, issue_position + 1
        .endr
    .else
        .irp r, 0,1,2,3
            .if (\idx) == \r
                ISSUE_INS_\cls \r
            .endif
        .endr
    .endif
.endm

// A slot is one class of a kernel: its chain base, chain count and the
// rotating counter.
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

// Default chains of a slot when its bank is not shared with another slot.
.macro ISSUE_SLOT_DEFAULT slot, cls
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_SET_SLOT \slot, 8, 16
    .elseif ISSUE_BANK_\cls == ISSUE_BANK_GPR
        ISSUE_SET_SLOT \slot, 0, 16
    .else
        ISSUE_SET_SLOT \slot, 0, 4
    .endif
.endm

// Two vector classes share v8-v31: the class with more instructions per
// pattern gets 16 chains (v8-v23), the other 8 (v24-v31).
.macro ISSUE_SPLIT_VEC major, minor
    ISSUE_SET_SLOT \major, 8, 16
    ISSUE_SET_SLOT \minor, 24, 8
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
        ISSUE_SET_SLOT A, 8, 8
        ISSUE_SET_SLOT B, 16, 8
        ISSUE_SET_SLOT C, 24, 8
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

// Function entry: save callee-saved state, set constants, enter the mode.
// Vector registers are zeroed so that FMLA chains stay finite; smstart does
// that itself in streaming mode.
.macro ISSUE_KERNEL_BEGIN name, mode, sve
    PROLOGUE \name
    preserve_caller_vec
    mov x2, #3
    .if \mode == ISSUE_MODE_STREAMING
#ifdef _SME_
        smstart
        ptrue p0.s
        ptrue p1.s
#endif
    .else
        INIT
#if defined(_SVE_) || defined(_SME_)
        // SVE classes: whole-register zero, so no lane holds a denormal.
        .if \sve
            ptrue p0.s
            .irp r, 0,1,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
                ISSUE_ZERO_Z \r
            .endr
        .endif
#endif
    .endif
1:
.endm

.macro ISSUE_KERNEL_END mode
    subs x1, x1, #1
    b.ne 1b
    .if \mode == ISSUE_MODE_STREAMING
#ifdef _SME_
        smstop
#endif
    .endif
    restore_caller_vec
    ret
.endm

// One pattern of a A's and b B's, spread evenly (weighted round robin).
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

// ---------------------------------------------------------------------------
// Kernel shapes.
// ---------------------------------------------------------------------------

// issue_<cls>_pure_<sfx>: ISSUE_PURE_BODY instructions over `chains` chains.
.macro ISSUE_PURE_KERNEL cls, sfx, chains
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_SET_SLOT A, 8, \chains
    .else
        ISSUE_SET_SLOT A, 0, \chains
    .endif
    ISSUE_KERNEL_BEGIN issue_\cls\()_pure_\sfx, ISSUE_MODE_\cls, ISSUE_SVE_\cls
    .rept ISSUE_PURE_BODY
        ISSUE_STEP A, \cls
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\cls
.endm

// Half and full chain counts of the class's bank.
.macro ISSUE_PURE_KERNELS cls
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_PURE_KERNEL \cls, h, 12
        ISSUE_PURE_KERNEL \cls, f, 24
    .elseif ISSUE_BANK_\cls == ISSUE_BANK_GPR
        ISSUE_PURE_KERNEL \cls, h, 8
        ISSUE_PURE_KERNEL \cls, f, 16
    .else
        ISSUE_PURE_KERNEL \cls, h, 2
        ISSUE_PURE_KERNEL \cls, f, 4
    .endif
.endm

// issue_<A>_<B>_<a>x<b>_il / _bl: the a:b pattern repeated, interleaved or in
// blocks of four patterns' worth of each class.
.macro ISSUE_PAIR_KERNEL A, B, a, b
    .if ISSUE_MODE_\A != ISSUE_MODE_\B
        .error "issue probe: \A and \B run in different modes and cannot mix"
    .endif
    ISSUE_PARTITION2 \A, \B, \a, \b
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\a\()x\b\()_il, ISSUE_MODE_\A, (ISSUE_SVE_\A | ISSUE_SVE_\B)
    .rept ISSUE_MIX_REPEATS(\a + \b)
        ISSUE_PATTERN2 \A, \B, \a, \b
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\A

    ISSUE_PARTITION2 \A, \B, \a, \b
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\a\()x\b\()_bl, ISSUE_MODE_\A, (ISSUE_SVE_\A | ISSUE_SVE_\B)
    .rept ISSUE_MIX_REPEATS(\a + \b) / 4
        .rept 4 * \a
            ISSUE_STEP A, \A
        .endr
        .rept 4 * \b
            ISSUE_STEP B, \B
        .endr
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\A
.endm

// issue_<A>_<B>_<C>_<a>x<b>x<c>_il: the a:b:c pattern repeated, interleaved.
.macro ISSUE_TRIPLE_KERNEL A, B, C, a, b, c
    .if (ISSUE_MODE_\A != ISSUE_MODE_\B) || (ISSUE_MODE_\A != ISSUE_MODE_\C)
        .error "issue probe: \A, \B and \C run in different modes and cannot mix"
    .endif
    ISSUE_PARTITION3 \A, \B, \C, \a, \b, \c
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\C\()_\a\()x\b\()x\c\()_il, ISSUE_MODE_\A, (ISSUE_SVE_\A | ISSUE_SVE_\B | ISSUE_SVE_\C)
    .rept ISSUE_MIX_REPEATS(\a + \b + \c)
        ISSUE_PATTERN3 \A, \B, \C, \a, \b, \c
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\A
.endm

// ---------------------------------------------------------------------------
// Expansion of the .def file: attributes, then pure kernels, then mixes.
// ---------------------------------------------------------------------------

#define ISSUE_CLASS(name, instruction, unit, bank, mode, sve, feature) \
    ISSUE_CLASS_ATTR name, ISSUE_BANK_##bank, ISSUE_MODE_##mode, sve
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE

#define ISSUE_CLASS(name, instruction, unit, bank, mode, sve, feature) \
    ISSUE_PURE_KERNELS name
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE

#define ISSUE_ASM_PAIR(A, B, a, b) ISSUE_PAIR_KERNEL A, B, a, b
#define ISSUE_ASM_TRIPLE(A, B, C, a, b, c) ISSUE_TRIPLE_KERNEL A, B, C, a, b, c
#define ISSUE_CLASS(name, instruction, unit, bank, mode, sve, feature)
#define ISSUE_PAIR(A, B, label) ISSUE_RATIO_GRID(ISSUE_ASM_PAIR, A, B, ;)
#define ISSUE_TRIPLE(A, B, C, label) ISSUE_TRIPLE_GRID(ISSUE_ASM_TRIPLE, A, B, C, ;)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
#undef ISSUE_ASM_PAIR
#undef ISSUE_ASM_TRIPLE
