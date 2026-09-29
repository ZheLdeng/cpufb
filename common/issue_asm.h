/* Assembler framework of the issue-probe kernels, shared by every
 * <arch>/asm/_ISSUE_.S.  It is a header of GAS macros, read through the C
 * preprocessor like the rest of the .S file, and it ends by expanding the
 * architecture's issue_classes.def into kernels.
 *
 * An architecture file defines, before including this header:
 *
 *   ISSUE_INS_<class> r          one instruction of that class on register r
 *   ISSUE_EMIT cls, bank, idx    maps chain idx of a bank to a register and
 *                                emits ISSUE_INS_<cls> on it
 *   ISSUE_SLOT_PURE slot, cls, chains
 *   ISSUE_SLOT_DEFAULT slot, cls a class alone in its bank
 *   ISSUE_SPLIT_VEC major, minor two vector classes sharing the bank
 *   ISSUE_SPLIT_VEC3             three vector classes sharing the bank
 *   ISSUE_HALF_/FULL_{VEC,GPR,ZA} chain counts of the two pure kernels
 *   ISSUE_COMBINE_ATTR2 A, B / ISSUE_COMBINE_ATTR3 A, B, C
 *                                set issue_attr from the classes' attributes
 *   ISSUE_KERNEL_BEGIN name, mode, attr / ISSUE_KERNEL_END mode, attr
 *
 * The ratio grids of issue_layout.h are handed to the assembler as ONE macro
 * call with a variadic tail (ISSUE_PAIRS A, B, 1, 7, 1, 6, ...), consumed one
 * ratio per recursion.  The preprocessor cannot emit line breaks, and a ';'
 * between entries is a statement separator only in GNU as: the Apple
 * assembler reads it as a comment and dropped 219 of 242 kernels.
 */

#ifndef CPUFB_ISSUE_ASM_H
#define CPUFB_ISSUE_ASM_H

#include "issue_layout.h"

/* Class attributes as assembler symbols. */
.macro ISSUE_CLASS_ATTR name, bank, mode, attr
    .set ISSUE_BANK_\name, \bank
    .set ISSUE_MODE_\name, \mode
    .set ISSUE_ATTR_\name, \attr
.endm

/* A slot is one class of a kernel: chain base, chain count, rotating counter. */
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

/* Register partition of a pair: the class with more instructions per
 * pattern is the major one when both share the vector bank. */
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
        ISSUE_SPLIT_VEC3
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

/* One pattern, the classes spread evenly (weighted round robin). */
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

/* ----- kernel shapes ----------------------------------------------------- */

/* issue_<cls>_pure_<sfx>: ISSUE_PURE_BODY instructions over `chains` chains. */
.macro ISSUE_PURE_KERNEL cls, sfx, chains
    ISSUE_SLOT_PURE A, \cls, \chains
    ISSUE_KERNEL_BEGIN issue_\cls\()_pure_\sfx, ISSUE_MODE_\cls, ISSUE_ATTR_\cls
    .rept ISSUE_PURE_BODY
        ISSUE_STEP A, \cls
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\cls, ISSUE_ATTR_\cls
.endm

.macro ISSUE_PURE_KERNELS cls
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_PURE_KERNEL \cls, h, ISSUE_HALF_VEC
        ISSUE_PURE_KERNEL \cls, f, ISSUE_FULL_VEC
    .elseif ISSUE_BANK_\cls == ISSUE_BANK_GPR
        ISSUE_PURE_KERNEL \cls, h, ISSUE_HALF_GPR
        ISSUE_PURE_KERNEL \cls, f, ISSUE_FULL_GPR
    .else
        ISSUE_PURE_KERNEL \cls, h, ISSUE_HALF_ZA
        ISSUE_PURE_KERNEL \cls, f, ISSUE_FULL_ZA
    .endif
.endm

/* issue_<A>_<B>_<a>x<b>_il / _bl: the a:b pattern repeated, interleaved or
 * in blocks of four patterns' worth of each class. */
.macro ISSUE_PAIR_KERNEL A, B, a, b
    .if ISSUE_MODE_\A != ISSUE_MODE_\B
        .error "issue probe: \A and \B run in different modes and cannot mix"
    .endif
    ISSUE_COMBINE_ATTR2 \A, \B
    ISSUE_PARTITION2 \A, \B, \a, \b
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\a\()x\b\()_il, ISSUE_MODE_\A, issue_attr
    .rept ISSUE_MIX_REPEATS(\a + \b)
        ISSUE_PATTERN2 \A, \B, \a, \b
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\A, issue_attr

    ISSUE_PARTITION2 \A, \B, \a, \b
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\a\()x\b\()_bl, ISSUE_MODE_\A, issue_attr
    .rept ISSUE_MIX_REPEATS(\a + \b) / 4
        .rept 4 * \a
            ISSUE_STEP A, \A
        .endr
        .rept 4 * \b
            ISSUE_STEP B, \B
        .endr
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\A, issue_attr
.endm

/* issue_<A>_<B>_<C>_<a>x<b>x<c>_il: the a:b:c pattern repeated, interleaved. */
.macro ISSUE_TRIPLE_KERNEL A, B, C, a, b, c
    .if (ISSUE_MODE_\A != ISSUE_MODE_\B) || (ISSUE_MODE_\A != ISSUE_MODE_\C)
        .error "issue probe: \A, \B and \C run in different modes and cannot mix"
    .endif
    ISSUE_COMBINE_ATTR3 \A, \B, \C
    ISSUE_PARTITION3 \A, \B, \C, \a, \b, \c
    ISSUE_KERNEL_BEGIN issue_\A\()_\B\()_\C\()_\a\()x\b\()x\c\()_il, ISSUE_MODE_\A, issue_attr
    .rept ISSUE_MIX_REPEATS(\a + \b + \c)
        ISSUE_PATTERN3 \A, \B, \C, \a, \b, \c
    .endr
    ISSUE_KERNEL_END ISSUE_MODE_\A, issue_attr
.endm

/* The whole grid of one pair or triple, one ratio per recursion. */
.macro ISSUE_PAIRS A, B, a, b, rest:vararg
    ISSUE_PAIR_KERNEL \A, \B, \a, \b
    .ifnb \rest
        ISSUE_PAIRS \A, \B, \rest
    .endif
.endm

.macro ISSUE_TRIPLES A, B, C, a, b, c, rest:vararg
    ISSUE_TRIPLE_KERNEL \A, \B, \C, \a, \b, \c
    .ifnb \rest
        ISSUE_TRIPLES \A, \B, \C, \rest
    .endif
.endm

/* ----- expansion of issue_classes.def: attributes, pure kernels, mixes --- */

#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature) \
    ISSUE_CLASS_ATTR name, ISSUE_BANK_##bank, ISSUE_MODE_##mode, attr
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE

#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature) \
    ISSUE_PURE_KERNELS name
#define ISSUE_PAIR(A, B, label)
#define ISSUE_TRIPLE(A, B, C, label)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE

#define ISSUE_RATIO_ARGS(A, B, a, b) a, b
#define ISSUE_TRIPLE_ARGS(A, B, C, a, b, c) a, b, c
#define ISSUE_CLASS(name, instruction, unit, bank, mode, attr, feature)
#define ISSUE_PAIR(A, B, label) \
    ISSUE_PAIRS A, B, ISSUE_RATIO_GRID(ISSUE_RATIO_ARGS, A, B, ISSUE_COMMA)
#define ISSUE_TRIPLE(A, B, C, label) \
    ISSUE_TRIPLES A, B, C, ISSUE_TRIPLE_GRID(ISSUE_TRIPLE_ARGS, A, B, C, ISSUE_COMMA)
#include "issue_classes.def"
#undef ISSUE_CLASS
#undef ISSUE_PAIR
#undef ISSUE_TRIPLE
#undef ISSUE_RATIO_ARGS
#undef ISSUE_TRIPLE_ARGS

#endif
