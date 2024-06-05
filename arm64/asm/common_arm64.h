#ifndef COMMON_ARM64
#define COMMON_ARM64

.macro PROLOGUE FUNCTION_NAME
.align 4
#ifdef __APPLE__
.global _\FUNCTION_NAME

_\FUNCTION_NAME:
#else
.global \FUNCTION_NAME

\FUNCTION_NAME:
#endif
.endm

.macro preserve_caller_vec
	stp d8, d9, [sp, #-16]!
	stp d10, d11, [sp, #-16]!
	stp d12, d13, [sp, #-16]!
	stp d14, d15, [sp, #-16]!
.endm

.macro restore_caller_vec
	ldp d14, d15, [sp], #16
	ldp d12, d13, [sp], #16
	ldp d10, d11, [sp], #16
	ldp d8, d9, [sp], #16
.endm

.macro INIT
    eor v0.16b, v0.16b, v0.16b
    eor v1.16b, v1.16b, v1.16b
    eor v8.16b, v8.16b, v8.16b
    eor v9.16b, v9.16b, v9.16b
    eor v10.16b, v10.16b, v10.16b
    eor v11.16b, v11.16b, v11.16b
    eor v12.16b, v12.16b, v12.16b
    eor v13.16b, v13.16b, v13.16b
    eor v14.16b, v14.16b, v14.16b
    eor v15.16b, v15.16b, v15.16b
    eor v16.16b, v16.16b, v16.16b
    eor v17.16b, v17.16b, v17.16b
    eor v18.16b, v18.16b, v18.16b
    eor v19.16b, v19.16b, v19.16b
    eor v20.16b, v20.16b, v20.16b
    eor v21.16b, v21.16b, v21.16b
    eor v22.16b, v22.16b, v22.16b
    eor v23.16b, v23.16b, v23.16b
    eor v24.16b, v24.16b, v24.16b
    eor v25.16b, v25.16b, v25.16b
    eor v26.16b, v26.16b, v26.16b
    eor v27.16b, v27.16b, v27.16b
    eor v28.16b, v28.16b, v28.16b
    eor v29.16b, v29.16b, v29.16b
    eor v30.16b, v30.16b, v30.16b
    eor v31.16b, v31.16b, v31.16b
.endm

#ifdef _SVE_FMLA_
.macro SVE_INIT
    fmov z0.b, p0/m, #0
    fmov z1.b, p0/m, #0
    fmov z8.b, p0/m, #0
    fmov z9.b, p0/m, #0
    fmov z10.b, p0/m, #0
    fmov z11.b, p0/m, #0
    fmov z12.b, p0/m, #0
    fmov z13.b, p0/m, #0
    fmov z14.b, p0/m, #0
    fmov z15.b, p0/m, #0
    fmov z16.b, p0/m, #0
    fmov z17.b, p0/m, #0
    fmov z18.b, p0/m, #0
    fmov z19.b, p0/m, #0
    fmov z20.b, p0/m, #0
    fmov z21.b, p0/m, #0
    fmov z22.b, p0/m, #0
    fmov z23.b, p0/m, #0
    fmov z24.b, p0/m, #0
    fmov z25.b, p0/m, #0
    fmov z26.b, p0/m, #0
    fmov z27.b, p0/m, #0
    fmov z28.b, p0/m, #0
    fmov z29.b, p0/m, #0
    fmov z30.b, p0/m, #0
    fmov z31.b, p0/m, #0
.endm

#endif


#endif