.align 4

.macro preserve_caller_vec
    csrr x5, vtype
    csrr x6, vl
    vsetvli x7, x0, e8, m8
    sub sp, sp, x7
    vse8.v v0, (sp)
    sub sp, sp, x7
    vse8.v v8, (sp)
    sub sp, sp, x7
    vse8.v v16, (sp)
    sub sp, sp, x7
    vse8.v v24, (sp)
.endm

.macro restore_caller_vec
    vsetvli x7, x0, e8, m8
    vle8.v v24, (sp)
    add sp, sp, x7
    vle8.v v16, (sp)
    add sp, sp, x7
    vle8.v v8, (sp)
    add sp, sp, x7
    vle8.v v0, (sp)
    add sp, sp, x7
    vsetvl x7, x6, x5
.endm

.globl riscv_vector_length_bytes
riscv_vector_length_bytes:
    vsetvli a0, x0, e8, m1
    ret

.globl riscv_cache_chase
riscv_cache_chase:
    li x5, 0
.riscv.cache.chase.loop:
    slli x6, x5, 3
    add x6, a1, x6
    ld x5, (x6)
    addi a0, a0, -1
    bnez a0, .riscv.cache.chase.loop
    ret

# void riscv_cache_chase32(int looptime, const int32_t *ptr): the same walk
# over 32-bit indices, so that a 64-byte line holds 16 links.  The indices are
# positive and below 2^28, so the sign extension lw performs is harmless.
.globl riscv_cache_chase32
riscv_cache_chase32:
    li x5, 0
.riscv.cache.chase32.loop:
    slli x6, x5, 2
    add x6, a1, x6
    lw x5, (x6)
    addi a0, a0, -1
    bnez a0, .riscv.cache.chase32.loop
    ret

.globl vector_load_stream
vector_load_stream:
    preserve_caller_vec
    vsetvli x28, x0, e8, m1
    add x29, a0, a1
.vector.load.stream.outer:
    mv x30, a0
.vector.load.stream.inner:
    vle8.v v0, (x30)
    add x30, x30, x28
    bltu x30, x29, .vector.load.stream.inner
    addi a2, a2, -1
    bnez a2, .vector.load.stream.outer
    restore_caller_vec
    ret

.globl vector_load_fma_stream
vector_load_fma_stream:
    preserve_caller_vec
    vsetvli x28, x0, e32, m1
    slli x31, x28, 2
    add x29, a0, a1
    vxor.vv v8, v8, v8
.vector.load.fma.stream.outer:
    mv x30, a0
.vector.load.fma.stream.inner:
    vle32.v v0, (x30)
    vfmacc.vv v8, v0, v0
    add x30, x30, x31
    bltu x30, x29, .vector.load.fma.stream.inner
    addi a2, a2, -1
    bnez a2, .vector.load.fma.stream.outer
    restore_caller_vec
    ret

#ifdef __APPLE__
.globl _vector_vfmacc_vf_f16f16f16
_vector_vfmacc_vf_f16f16f16:
#else
.globl vector_vfmacc_vf_f16f16f16
vector_vfmacc_vf_f16f16f16:
#endif
    preserve_caller_vec
    vsetvli x7, x0, e8, m8
    vxor.vv v0, v0, v0
    vxor.vv v8, v8, v8
    vxor.vv v16, v16, v16
    vxor.vv v24, v24, v24
.vector.vfmacc.vf.f16f16f16.L1:
    vsetvli x7, x0, e16, m1
    vfmacc.vf v9, f0, v1
    vfmacc.vf v10, f0, v1
    vfmacc.vf v11, f0, v1
    vfmacc.vf v12, f0, v1
    vfmacc.vf v13, f0, v1
    vfmacc.vf v14, f0, v1
    vfmacc.vf v15, f0, v1
    vfmacc.vf v16, f0, v1
    vfmacc.vf v17, f0, v1
    vfmacc.vf v18, f0, v1
    vfmacc.vf v19, f0, v1
    addi a0, a0, -1
    vfmacc.vf v20, f0, v1
    vfmacc.vf v21, f0, v1
    vfmacc.vf v22, f0, v1
    vfmacc.vf v23, f0, v1
    vfmacc.vf v24, f0, v1
    vfmacc.vf v25, f0, v1
    vfmacc.vf v26, f0, v1
    vfmacc.vf v27, f0, v1
    vfmacc.vf v28, f0, v1
    vfmacc.vf v29, f0, v1
    vfmacc.vf v30, f0, v1
    vfmacc.vf v31, f0, v1
    bnez a0, .vector.vfmacc.vf.f16f16f16.L1
    restore_caller_vec
    ret

#ifdef __APPLE__
.globl _vector_vfmacc_vv_f16f16f16
_vector_vfmacc_vv_f16f16f16:
#else
.globl vector_vfmacc_vv_f16f16f16
vector_vfmacc_vv_f16f16f16:
#endif
    preserve_caller_vec
    vsetvli x7, x0, e8, m8
    vxor.vv v0, v0, v0
    vxor.vv v8, v8, v8
    vxor.vv v16, v16, v16
    vxor.vv v24, v24, v24
.vector.vfmacc.vv.f16f16f16.L1:
    vsetvli x7, x0, e16, m1
    vfmacc.vv v8, v0, v1
    vfmacc.vv v9, v0, v1
    vfmacc.vv v10, v0, v1
    vfmacc.vv v11, v0, v1
    vfmacc.vv v12, v0, v1
    vfmacc.vv v13, v0, v1
    vfmacc.vv v14, v0, v1
    vfmacc.vv v15, v0, v1
    vfmacc.vv v16, v0, v1
    vfmacc.vv v17, v0, v1
    vfmacc.vv v18, v0, v1
    vfmacc.vv v19, v0, v1
    addi a0, a0, -1
    vfmacc.vv v20, v0, v1
    vfmacc.vv v21, v0, v1
    vfmacc.vv v22, v0, v1
    vfmacc.vv v23, v0, v1
    vfmacc.vv v24, v0, v1
    vfmacc.vv v25, v0, v1
    vfmacc.vv v26, v0, v1
    vfmacc.vv v27, v0, v1
    vfmacc.vv v28, v0, v1
    vfmacc.vv v29, v0, v1
    vfmacc.vv v30, v0, v1
    vfmacc.vv v31, v0, v1
    bnez a0, .vector.vfmacc.vv.f16f16f16.L1
    restore_caller_vec
    ret

#ifdef __APPLE__
.globl _vector_vfmacc_vf_f32f32f32
_vector_vfmacc_vf_f32f32f32:
#else
.globl vector_vfmacc_vf_f32f32f32
vector_vfmacc_vf_f32f32f32:
#endif
    preserve_caller_vec
    vsetvli x7, x0, e8, m8
    vxor.vv v0, v0, v0
    vxor.vv v8, v8, v8
    vxor.vv v16, v16, v16
    vxor.vv v24, v24, v24
.vector.vfmacc.vf.f32f32f32.L1:
    vsetvli x7, x0, e32, m1
    vfmacc.vf v9, f0, v1
    vfmacc.vf v10, f0, v1
    vfmacc.vf v11, f0, v1
    vfmacc.vf v12, f0, v1
    vfmacc.vf v13, f0, v1
    vfmacc.vf v14, f0, v1
    vfmacc.vf v15, f0, v1
    vfmacc.vf v16, f0, v1
    vfmacc.vf v17, f0, v1
    vfmacc.vf v18, f0, v1
    vfmacc.vf v19, f0, v1
    addi a0, a0, -1
    vfmacc.vf v20, f0, v1
    vfmacc.vf v21, f0, v1
    vfmacc.vf v22, f0, v1
    vfmacc.vf v23, f0, v1
    vfmacc.vf v24, f0, v1
    vfmacc.vf v25, f0, v1
    vfmacc.vf v26, f0, v1
    vfmacc.vf v27, f0, v1
    vfmacc.vf v28, f0, v1
    vfmacc.vf v29, f0, v1
    vfmacc.vf v30, f0, v1
    vfmacc.vf v31, f0, v1
    bnez a0, .vector.vfmacc.vf.f32f32f32.L1
    restore_caller_vec
    ret

#ifdef __APPLE__
.globl _vector_vfmacc_vv_f32f32f32
_vector_vfmacc_vv_f32f32f32:
#else
.globl vector_vfmacc_vv_f32f32f32
vector_vfmacc_vv_f32f32f32:
#endif
    preserve_caller_vec
    vsetvli x7, x0, e8, m8
    vxor.vv v0, v0, v0
    vxor.vv v8, v8, v8
    vxor.vv v16, v16, v16
    vxor.vv v24, v24, v24
.vector.vfmacc.vv.f32f32f32.L1:
    vsetvli x7, x0, e32, m1
    vfmacc.vv v8, v0, v1
    vfmacc.vv v9, v0, v1
    vfmacc.vv v10, v0, v1
    vfmacc.vv v11, v0, v1
    vfmacc.vv v12, v0, v1
    vfmacc.vv v13, v0, v1
    vfmacc.vv v14, v0, v1
    vfmacc.vv v15, v0, v1
    vfmacc.vv v16, v0, v1
    vfmacc.vv v17, v0, v1
    vfmacc.vv v18, v0, v1
    vfmacc.vv v19, v0, v1
    addi a0, a0, -1
    vfmacc.vv v20, v0, v1
    vfmacc.vv v21, v0, v1
    vfmacc.vv v22, v0, v1
    vfmacc.vv v23, v0, v1
    vfmacc.vv v24, v0, v1
    vfmacc.vv v25, v0, v1
    vfmacc.vv v26, v0, v1
    vfmacc.vv v27, v0, v1
    vfmacc.vv v28, v0, v1
    vfmacc.vv v29, v0, v1
    vfmacc.vv v30, v0, v1
    vfmacc.vv v31, v0, v1
    bnez a0, .vector.vfmacc.vv.f32f32f32.L1
    restore_caller_vec
    ret

#ifdef __APPLE__
.globl _vector_vfmacc_vf_f64f64f64
_vector_vfmacc_vf_f64f64f64:
#else
.globl vector_vfmacc_vf_f64f64f64
vector_vfmacc_vf_f64f64f64:
#endif
    preserve_caller_vec
    vsetvli x7, x0, e8, m8
    vxor.vv v0, v0, v0
    vxor.vv v8, v8, v8
    vxor.vv v16, v16, v16
    vxor.vv v24, v24, v24
.vector.vfmacc.vf.f64f64f64.L1:
    vsetvli x7, x0, e64, m1
    vfmacc.vf v9, f0, v1
    vfmacc.vf v10, f0, v1
    vfmacc.vf v11, f0, v1
    vfmacc.vf v12, f0, v1
    vfmacc.vf v13, f0, v1
    vfmacc.vf v14, f0, v1
    vfmacc.vf v15, f0, v1
    vfmacc.vf v16, f0, v1
    vfmacc.vf v17, f0, v1
    vfmacc.vf v18, f0, v1
    vfmacc.vf v19, f0, v1
    addi a0, a0, -1
    vfmacc.vf v20, f0, v1
    vfmacc.vf v21, f0, v1
    vfmacc.vf v22, f0, v1
    vfmacc.vf v23, f0, v1
    vfmacc.vf v24, f0, v1
    vfmacc.vf v25, f0, v1
    vfmacc.vf v26, f0, v1
    vfmacc.vf v27, f0, v1
    vfmacc.vf v28, f0, v1
    vfmacc.vf v29, f0, v1
    vfmacc.vf v30, f0, v1
    vfmacc.vf v31, f0, v1
    bnez a0, .vector.vfmacc.vf.f64f64f64.L1
    restore_caller_vec
    ret

#ifdef __APPLE__
.globl _vector_vfmacc_vv_f64f64f64
_vector_vfmacc_vv_f64f64f64:
#else
.globl vector_vfmacc_vv_f64f64f64
vector_vfmacc_vv_f64f64f64:
#endif
    preserve_caller_vec
    vsetvli x7, x0, e8, m8
    vxor.vv v0, v0, v0
    vxor.vv v8, v8, v8
    vxor.vv v16, v16, v16
    vxor.vv v24, v24, v24
.vector.vfmacc.vv.f64f64f64.L1:
    vsetvli x7, x0, e64, m1
    vfmacc.vv v8, v0, v1
    vfmacc.vv v9, v0, v1
    vfmacc.vv v10, v0, v1
    vfmacc.vv v11, v0, v1
    vfmacc.vv v12, v0, v1
    vfmacc.vv v13, v0, v1
    vfmacc.vv v14, v0, v1
    vfmacc.vv v15, v0, v1
    vfmacc.vv v16, v0, v1
    vfmacc.vv v17, v0, v1
    vfmacc.vv v18, v0, v1
    vfmacc.vv v19, v0, v1
    addi a0, a0, -1
    vfmacc.vv v20, v0, v1
    vfmacc.vv v21, v0, v1
    vfmacc.vv v22, v0, v1
    vfmacc.vv v23, v0, v1
    vfmacc.vv v24, v0, v1
    vfmacc.vv v25, v0, v1
    vfmacc.vv v26, v0, v1
    vfmacc.vv v27, v0, v1
    vfmacc.vv v28, v0, v1
    vfmacc.vv v29, v0, v1
    vfmacc.vv v30, v0, v1
    vfmacc.vv v31, v0, v1
    bnez a0, .vector.vfmacc.vv.f64f64f64.L1
    restore_caller_vec
    ret
