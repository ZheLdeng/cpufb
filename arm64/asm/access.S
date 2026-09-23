.align 5
#include "common_arm64.h"

#void load_ptr(int looptime, int64_t *ptr)
PROLOGUE load_ptr
    mov w2, w0
    mov w3, #0
.loadL1:
    ldr x3, [x1, x3, lsl #3]
    subs x2, x2, #1
    bne .loadL1
    ret

# void load_ptr32(int looptime, const int32_t *ptr): the same walk over
# 32-bit indices, so that a 64-byte line holds 16 links (see cache_curve.cpp).
PROLOGUE load_ptr32
    mov w2, w0
    mov w3, #0
.loadPtr32L1:
    ldr w3, [x1, x3, lsl #2]
    subs x2, x2, #1
    bne .loadPtr32L1
    ret

# uint64_t clock_add_chain(int64_t looptime, uint64_t addend)
# Core-clock calibration without PMU access: a register-register ADD
# dependency chain retires exactly one ADD per core cycle, so
# 16 * looptime / elapsed is the running core clock.  The addend is a run-time
# register value so the chain cannot be folded; loop control is off the chain.
PROLOGUE clock_add_chain
    mov x2, #0
.clockAddChainL1:
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    add x2, x2, x1
    subs x0, x0, #1
    bne .clockAddChainL1
    mov x0, x2
    ret
