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
