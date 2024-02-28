	.arch armv8-a
	.file	"cpufp_kernel_armv8.c"
	.text
	.align	2
	.global	cpufp_kernel_armv8_fmla_f32
	.type	cpufp_kernel_armv8_fmla_f32, %function
cpufp_kernel_armv8_fmla_f32:
.LFB6:
	.cfi_startproc
	sub	sp, sp, #32
	.cfi_def_cfa_offset 32
	str	x0, [sp, 8]
	str	wzr, [sp, 28]
	b	.L2
.L3:
#APP
// 8 "cpufp_kernel_armv8.c" 1
	fmla v0.4s, v0.4s, v0.4s
	fmla v1.4s, v1.4s, v1.4s
	fmla v2.4s, v2.4s, v2.4s
	fmla v3.4s, v3.4s, v3.4s
	fmla v4.4s, v4.4s, v4.4s
	fmla v5.4s, v5.4s, v5.4s
	fmla v6.4s, v6.4s, v6.4s
	fmla v7.4s, v7.4s, v7.4s
	fmla v8.4s, v8.4s, v8.4s
	fmla v9.4s, v9.4s, v9.4s
	fmla v10.4s, v10.4s, v10.4s
	fmla v11.4s, v11.4s, v11.4s
	fmla v12.4s, v12.4s, v12.4s
	fmla v13.4s, v13.4s, v13.4s
	fmla v14.4s, v14.4s, v14.4s
	fmla v15.4s, v15.4s, v15.4s
	fmla v16.4s, v16.4s, v16.4s
	fmla v17.4s, v17.4s, v17.4s
	fmla v18.4s, v18.4s, v18.4s
	fmla v19.4s, v19.4s, v19.4s
	fmla v20.4s, v20.4s, v20.4s
	fmla v21.4s, v21.4s, v21.4s
	fmla v22.4s, v22.4s, v22.4s
	fmla v23.4s, v23.4s, v23.4s
	fmla v24.4s, v24.4s, v24.4s
	fmla v25.4s, v25.4s, v25.4s
	fmla v26.4s, v26.4s, v26.4s
	fmla v27.4s, v27.4s, v27.4s
	fmla v28.4s, v28.4s, v28.4s
	fmla v29.4s, v29.4s, v29.4s
	fmla v30.4s, v30.4s, v30.4s
	fmla v31.4s, v31.4s, v31.4s
	
// 0 "" 2
#NO_APP
	ldr	w0, [sp, 28]
	add	w0, w0, 1
	str	w0, [sp, 28]
.L2:
	ldrsw	x0, [sp, 28]
	ldr	x1, [sp, 8]
	cmp	x1, x0
	bgt	.L3
	nop
	nop
	add	sp, sp, 32
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE6:
	.size	cpufp_kernel_armv8_fmla_f32, .-cpufp_kernel_armv8_fmla_f32
	.align	2
	.global	cpufp_kernel_armv8_fmla_f64
	.type	cpufp_kernel_armv8_fmla_f64, %function
cpufp_kernel_armv8_fmla_f64:
.LFB7:
	.cfi_startproc
	sub	sp, sp, #32
	.cfi_def_cfa_offset 32
	str	x0, [sp, 8]
	str	wzr, [sp, 28]
	b	.L5
.L6:
#APP
// 55 "cpufp_kernel_armv8.c" 1
	fmla v0.2d, v0.2d, v0.2d
	fmla v1.2d, v1.2d, v1.2d
	fmla v2.2d, v2.2d, v2.2d
	fmla v3.2d, v3.2d, v3.2d
	fmla v4.2d, v4.2d, v4.2d
	fmla v5.2d, v5.2d, v5.2d
	fmla v6.2d, v6.2d, v6.2d
	fmla v7.2d, v7.2d, v7.2d
	fmla v8.2d, v8.2d, v8.2d
	fmla v9.2d, v9.2d, v9.2d
	fmla v10.2d, v10.2d, v10.2d
	fmla v11.2d, v11.2d, v11.2d
	fmla v12.2d, v12.2d, v12.2d
	fmla v13.2d, v13.2d, v13.2d
	fmla v14.2d, v14.2d, v14.2d
	fmla v15.2d, v15.2d, v15.2d
	fmla v16.2d, v16.2d, v16.2d
	fmla v17.2d, v17.2d, v17.2d
	fmla v18.2d, v18.2d, v18.2d
	fmla v19.2d, v19.2d, v19.2d
	fmla v20.2d, v20.2d, v20.2d
	fmla v21.2d, v21.2d, v21.2d
	fmla v22.2d, v22.2d, v22.2d
	fmla v23.2d, v23.2d, v23.2d
	fmla v24.2d, v24.2d, v24.2d
	fmla v25.2d, v25.2d, v25.2d
	fmla v26.2d, v26.2d, v26.2d
	fmla v27.2d, v27.2d, v27.2d
	fmla v28.2d, v28.2d, v28.2d
	fmla v29.2d, v29.2d, v29.2d
	fmla v30.2d, v30.2d, v30.2d
	fmla v31.2d, v31.2d, v31.2d
	
// 0 "" 2
#NO_APP
	ldr	w0, [sp, 28]
	add	w0, w0, 1
	str	w0, [sp, 28]
.L5:
	ldrsw	x0, [sp, 28]
	ldr	x1, [sp, 8]
	cmp	x1, x0
	bgt	.L6
	nop
	nop
	add	sp, sp, 32
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE7:
	.size	cpufp_kernel_armv8_fmla_f64, .-cpufp_kernel_armv8_fmla_f64
	.align	2
	.type	get_time, %function
get_time:
.LFB8:
	.cfi_startproc
	sub	sp, sp, #16
	.cfi_def_cfa_offset 16
	str	x0, [sp, 8]
	str	x1, [sp]
	ldr	x0, [sp]
	ldr	x1, [x0]
	ldr	x0, [sp, 8]
	ldr	x0, [x0]
	sub	x0, x1, x0
	fmov	d0, x0
	scvtf	d1, d0
	ldr	x0, [sp]
	ldr	x1, [x0, 8]
	ldr	x0, [sp, 8]
	ldr	x0, [x0, 8]
	sub	x0, x1, x0
	fmov	d0, x0
	scvtf	d0, d0
	adrp	x0, .LC0
	ldr	d2, [x0, #:lo12:.LC0]
	fmul	d0, d0, d2
	fadd	d0, d1, d0
	add	sp, sp, 16
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE8:
	.size	get_time, .-get_time
	.section	.rodata
	.align	3
.LC1:
	.string	"inner loop = %d, loop time = %d\n"
	.align	3
.LC2:
	.string	"start"
	.align	3
.LC3:
	.string	"loop = %d\n"
	.align	3
.LC4:
	.string	"inloop = %d\n"
	.align	3
.LC5:
	.string	"time = %.2f, perf = %.2f byte/cycle \n"
	.text
	.align	2
	.global	cpufp_kernel_neon_bandwith_l1cache
	.type	cpufp_kernel_neon_bandwith_l1cache, %function
cpufp_kernel_neon_bandwith_l1cache:
.LFB9:
	.cfi_startproc
	stp	x29, x30, [sp, -112]!
	.cfi_def_cfa_offset 112
	.cfi_offset 29, -112
	.cfi_offset 30, -104
	mov	x29, sp
	str	x0, [sp, 24]
	mov	w0, 8192
	movk	w0, 0x3, lsl 16
	str	w0, [sp, 100]
	ldrsw	x0, [sp, 100]
	bl	malloc
	str	x0, [sp, 88]
	ldrsw	x0, [sp, 100]
	lsr	x0, x0, 11
	str	w0, [sp, 84]
	ldr	x2, [sp, 24]
	ldr	w1, [sp, 84]
	adrp	x0, .LC1
	add	x0, x0, :lo12:.LC1
	bl	printf
	adrp	x0, .LC2
	add	x0, x0, :lo12:.LC2
	bl	puts
	add	x0, sp, 48
	mov	x1, x0
	mov	w0, 4
	bl	clock_gettime
	str	wzr, [sp, 108]
	b	.L10
.L13:
	ldr	w1, [sp, 108]
	adrp	x0, .LC3
	add	x0, x0, :lo12:.LC3
	bl	printf
	str	wzr, [sp, 104]
	b	.L11
.L12:
	ldr	w1, [sp, 104]
	adrp	x0, .LC4
	add	x0, x0, :lo12:.LC4
	bl	printf
	ldr	x0, [sp, 88]
	ldr	s1, [x0]
	movi	v0.2s, 0x43, lsl 24
	fadd	s0, s1, s0
	ldr	x0, [sp, 88]
	str	s0, [x0]
	ldr	x0, [sp, 88]
#APP
// 128 "cpufp_kernel_armv8.c" 1
	mov x0, x0
	ldr q0, [x0], #16
	ldr q1, [x0], #16
	ldr q2, [x0], #16
	ldr q3, [x0], #16
	ldr q4, [x0], #16
	ldr q5, [x0], #16
	ldr q6, [x0], #16
	ldr q7, [x0], #16
	ldr q8, [x0], #16
	ldr q9, [x0], #16
	ldr q10, [x0], #16
	ldr q11, [x0], #16
	ldr q12, [x0], #16
	ldr q13, [x0], #16
	ldr q14, [x0], #16
	ldr q15, [x0], #16
	ldr q16, [x0], #16
	ldr q17, [x0], #16
	ldr q18, [x0], #16
	ldr q19, [x0], #16
	ldr q20, [x0], #16
	ldr q21, [x0], #16
	ldr q22, [x0], #16
	ldr q23, [x0], #16
	ldr q24, [x0], #16
	ldr q25, [x0], #16
	ldr q26, [x0], #16
	ldr q27, [x0], #16
	ldr q28, [x0], #16
	ldr q29, [x0], #16
	ldr q30, [x0], #16
	ldr q31, [x0], #16
	
// 0 "" 2
#NO_APP
	ldr	w0, [sp, 104]
	add	w0, w0, 1
	str	w0, [sp, 104]
.L11:
	ldr	w1, [sp, 104]
	ldr	w0, [sp, 84]
	cmp	w1, w0
	blt	.L12
	ldr	w0, [sp, 108]
	add	w0, w0, 1
	str	w0, [sp, 108]
.L10:
	ldrsw	x0, [sp, 108]
	ldr	x1, [sp, 24]
	cmp	x1, x0
	bgt	.L13
	add	x0, sp, 32
	mov	x1, x0
	mov	w0, 4
	bl	clock_gettime
	add	x1, sp, 32
	add	x0, sp, 48
	bl	get_time
	str	d0, [sp, 72]
	ldr	x1, [sp, 24]
	mov	x0, x1
	lsl	x0, x0, 1
	add	x0, x0, x1
	lsl	x0, x0, 3
	add	x0, x0, x1
	lsl	x0, x0, 13
	fmov	d0, x0
	scvtf	d1, d0
	ldr	d0, [sp, 72]
	adrp	x0, .LC6
	ldr	d2, [x0, #:lo12:.LC6]
	fmul	d0, d0, d2
	mov	x0, 225833675390976
	movk	x0, 0x41cd, lsl 48
	fmov	d2, x0
	fmul	d0, d0, d2
	fdiv	d0, d1, d0
	str	d0, [sp, 64]
	ldr	d1, [sp, 64]
	ldr	d0, [sp, 72]
	adrp	x0, .LC5
	add	x0, x0, :lo12:.LC5
	bl	printf
	nop
	ldp	x29, x30, [sp], 112
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE9:
	.size	cpufp_kernel_neon_bandwith_l1cache, .-cpufp_kernel_neon_bandwith_l1cache
	.section	.rodata
	.align	3
.LC0:
	.word	-400107883
	.word	1041313291
	.align	3
.LC6:
	.word	-858993459
	.word	1074056396
	.ident	"GCC: (GNU) 13.2.0"
	.section	.note.GNU-stack,"",@progbits
