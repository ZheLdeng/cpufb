// Issue-probe kernels for ARM64.  The framework (slots, partitions,
// patterns, kernel shapes, the .def expansion) is common/issue_asm.h; this
// file supplies what is specific to the architecture: the instruction of
// each class, the register banks, the function entry and exit.
//
// Every kernel is
//     void issue_<...>(const void *l1_buffer /* x0 */, int64_t loops /* x1 */)
// x2 holds the ALU addend; v0/v1 (z0/z1) are constant zero operands.
// Adding a class takes one ISSUE_INS_<name> macro here and one line in
// arm64/kernel/issue_classes.def.  The macro receives the register number of
// the current chain.  The class attribute (6th .def field) is 1 when the
// kernel prologue must set up SVE predicates and zero the Z registers.

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
// Register banks.  Chain indices are positions in a bank's register list:
// vectors v8-v31 (24), general registers x9-x15 and x19-x27 (16), ZA tiles
// 0-3 (4).
// ---------------------------------------------------------------------------

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

// Pure kernels: half and full chain counts per bank.
    .set ISSUE_HALF_VEC, 12
    .set ISSUE_FULL_VEC, 24
    .set ISSUE_HALF_GPR, 8
    .set ISSUE_FULL_GPR, 16
    .set ISSUE_HALF_ZA, 2
    .set ISSUE_FULL_ZA, 4

.macro ISSUE_SLOT_PURE slot, cls, chains
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_SET_SLOT \slot, 8, \chains
    .else
        ISSUE_SET_SLOT \slot, 0, \chains
    .endif
.endm

// A class alone in its bank in a mix.
.macro ISSUE_SLOT_DEFAULT slot, cls
    .if ISSUE_BANK_\cls == ISSUE_BANK_VEC
        ISSUE_SET_SLOT \slot, 8, 16
    .elseif ISSUE_BANK_\cls == ISSUE_BANK_GPR
        ISSUE_SET_SLOT \slot, 0, 16
    .else
        ISSUE_SET_SLOT \slot, 0, 4
    .endif
.endm

// Two vector classes share v8-v31: the major one gets 16 chains (v8-v23),
// the other 8 (v24-v31); three get 8 each.
.macro ISSUE_SPLIT_VEC major, minor
    ISSUE_SET_SLOT \major, 8, 16
    ISSUE_SET_SLOT \minor, 24, 8
.endm

.macro ISSUE_SPLIT_VEC3
    ISSUE_SET_SLOT A, 8, 8
    ISSUE_SET_SLOT B, 16, 8
    ISSUE_SET_SLOT C, 24, 8
.endm

// The kernel attribute is the OR of the classes' (any SVE class: predicates).
.macro ISSUE_COMBINE_ATTR2 A, B
    .set issue_attr, ISSUE_ATTR_\A | ISSUE_ATTR_\B
.endm

.macro ISSUE_COMBINE_ATTR3 A, B, C
    .set issue_attr, ISSUE_ATTR_\A | ISSUE_ATTR_\B | ISSUE_ATTR_\C
.endm

// ---------------------------------------------------------------------------
// Function entry and exit.
// ---------------------------------------------------------------------------

// Zeroes one Z register.  A helper because inside a .irp nested in a macro
// the assembler only substitutes \r when a comma or line end follows it.
.macro ISSUE_ZERO_Z r
    dup z\r\().s, #0
.endm

// Save callee-saved state, set constants, enter the mode.  Vector registers
// are zeroed so that FMLA chains stay finite; smstart does that itself.
.macro ISSUE_KERNEL_BEGIN name, mode, attr
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
        .if \attr
            ptrue p0.s
            .irp r, 0,1,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
                ISSUE_ZERO_Z \r
            .endr
        .endif
#endif
    .endif
1:
.endm

.macro ISSUE_KERNEL_END mode, attr
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

#include "issue_asm.h"
