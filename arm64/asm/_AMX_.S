// Apple AMX kernels.  AMX is Apple's undocumented matrix coprocessor on the
// M1 to M3 generations; its instructions have no mnemonics, so each one is
// emitted as its 32-bit encoding: 0x00201000 | (op << 5) | Xn, where Xn holds
// the operand word (fields documented by the community reverse engineering,
// e.g. the z row group in bits 20-25, vector mode in bit 63).  AMX_SET and
// AMX_CLR take an immediate instead of a register and need three NOPs ahead
// of them.  Whether the instructions exist is decided at run time by
// amx_available() (kernel/amx.cpp), which executes amx_probe below under a
// SIGILL guard; a core without AMX registers no AMX row.
//
// Compute kernels: void amx_<name>(int64_t loops), eight independent
// instructions per loop on eight different Z row groups, like the eight-
// accumulator NEON/SVE kernels.  Load kernels: void amx_ldx_<n>_kernel(
// float *data, int blocks, int64_t passes), 512 bytes per block.

#include "common_arm64.h"

    .text

.macro AMX_OP op, reg
    .long 0x00201000 + (\op << 5) + \reg
.endm

.macro AMX_SET
    nop
    nop
    nop
    .long 0x00201000 + (17 << 5) + 0
.endm

.macro AMX_CLR
    nop
    nop
    nop
    .long 0x00201000 + (17 << 5) + 1
.endm

// Operand words in x1..x8: bits 63:48 in hi48, bits 47:32 in hi32, and the
// Z row group k in bits 20-25 (k << 4 in the 16-31 half-word).
.macro AMX_OPERANDS hi48, hi32
    .irp k, 1,2,3,4,5,6,7,8
    movz x\k, #\hi48, lsl #48
    movk x\k, #\hi32, lsl #32
    movk x\k, #((\k - 1) << 4), lsl #16
    .endr
.endm

// A compute kernel: eight instructions of \op per loop, operands as above.
.macro AMX_COMPUTE_KERNEL name, op, hi48, hi32
    PROLOGUE \name
    AMX_SET
    AMX_OPERANDS \hi48, \hi32
    1:
    .irp k, 1,2,3,4,5,6,7,8
    AMX_OP \op, \k
    .endr
    subs x0, x0, #1
    bne 1b
    AMX_CLR
    ret
.endm

// Does the core execute AMX instructions at all?  SET then CLR, nothing
// else; a SIGILL here is caught by amx_available().
    PROLOGUE amx_probe
    AMX_SET
    AMX_CLR
    ret

// fma16 (op 15), fma32 (op 12), fma64 (op 10): outer product in matrix mode
// (operand bit 63 clear), elementwise in vector mode (bit 63 set).
    AMX_COMPUTE_KERNEL amx_fma16_mat, 15, 0x0000, 0x0000
    AMX_COMPUTE_KERNEL amx_fma32_mat, 12, 0x0000, 0x0000
    AMX_COMPUTE_KERNEL amx_fma64_mat, 10, 0x0000, 0x0000
    AMX_COMPUTE_KERNEL amx_fma16_vec, 15, 0x8000, 0x0000
    AMX_COMPUTE_KERNEL amx_fma32_vec, 12, 0x8000, 0x0000
    AMX_COMPUTE_KERNEL amx_fma64_vec, 10, 0x8000, 0x0000

// matint (op 20): ALU mode in bits 47-52, lane width mode in bits 42-45.
// i8 x i8 -> i32: ALU 8 (ifma_alt), dtype 0;  i8 x i16 -> i32: ALU 8,
// dtype 9;  i16 x i16 -> i32: ALU 0 (ifma), dtype 0.
    AMX_COMPUTE_KERNEL amx_matint_i8i8, 20, 0x0004, 0x0000
    AMX_COMPUTE_KERNEL amx_matint_i8i16, 20, 0x0004, 0x0240
    AMX_COMPUTE_KERNEL amx_matint_i16i16, 20, 0x0000, 0x0000

// ldx (op 0): the operand is the address (bits 0-56) with the X register
// index in bits 56-58, bit 62 for a pair of registers and bits 62+60 for
// four.  One block is 512 bytes: eight single loads, four pairs or two
// quads, into registers 0-7.
.macro AMX_LDX_KERNEL name, loads, step, flags_hi
    PROLOGUE \name
    AMX_SET
    mov x4, x2
    .set amx_idx, 0
    .irp r, 10,11,12,13,14,15,16,17
    .if amx_idx < \loads
    movz x\r, #(\flags_hi + ((amx_idx * (8 / \loads)) << 8)), lsl #48
    .endif
    .set amx_idx, amx_idx + 1
    .endr
    2:
    mov x3, x1
    mov x5, x0
    1:
    .set amx_idx, 0
    .irp r, 10,11,12,13,14,15,16,17
    .if amx_idx < \loads
    orr x9, x5, x\r
    AMX_OP 0, 9
    add x5, x5, #\step
    .endif
    .set amx_idx, amx_idx + 1
    .endr
    subs x3, x3, #1
    bne 1b
    subs x4, x4, #1
    bne 2b
    AMX_CLR
    ret
.endm

    AMX_LDX_KERNEL amx_ldx_1_kernel, 8, 64, 0x0000
    AMX_LDX_KERNEL amx_ldx_2_kernel, 4, 128, 0x4000
    AMX_LDX_KERNEL amx_ldx_4_kernel, 2, 256, 0x5000
