.align 4
#include "common_arm64.h"

// NEON TBL / TBX with a 4-register table {v0-v3}.
//   tbl  vd.16b, {v0.16b - v3.16b}, vN.16b   ; per-lane table lookup
//   tbx  vd.16b, {v0.16b - v3.16b}, vN.16b   ; same but skip out-of-range
// Per .16b instruction:
//   * 16 lookup ops    -> dim "OPS"
//   * 16 bytes touched -> dim "Byte/Cycle"
// 24 unrolled instructions per loop  -> comp_pl = 384.

#void asimd_tbl_4table_v_u8(int64_t);
#void asimd_tbx_4table_v_u8(int64_t);

###############################
    PROLOGUE asimd_tbl_4table_v_u8
    preserve_caller_vec
    INIT
    // index vector v4: 0..15 covers the first 16-byte table (v0)
    movi v4.16b, #0
    eor  v5.16b, v5.16b, v5.16b
    eor  v6.16b, v6.16b, v6.16b
    eor  v7.16b, v7.16b, v7.16b
.asimd.tbl.4table.v.u8.L1:
    tbl v8.16b , {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v9.16b , {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v10.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v11.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v12.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v13.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v14.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v15.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v16.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v17.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v18.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v19.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    subs x0, x0, #1
    tbl v20.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v21.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v22.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v23.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v24.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v25.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v26.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v27.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v28.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v29.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v30.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbl v31.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    bne .asimd.tbl.4table.v.u8.L1
    restore_caller_vec
    ret

###############################
    PROLOGUE asimd_tbx_4table_v_u8
    preserve_caller_vec
    INIT
    movi v4.16b, #0
    eor  v5.16b, v5.16b, v5.16b
    eor  v6.16b, v6.16b, v6.16b
    eor  v7.16b, v7.16b, v7.16b
.asimd.tbx.4table.v.u8.L1:
    tbx v8.16b , {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v9.16b , {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v10.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v11.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v12.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v13.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v14.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v15.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v16.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v17.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v18.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v19.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    subs x0, x0, #1
    tbx v20.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v21.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v22.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v23.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v24.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v25.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v26.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v27.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v28.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v29.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v30.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    tbx v31.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v4.16b
    bne .asimd.tbx.4table.v.u8.L1
    restore_caller_vec
    ret

###############################
// Latency variants: Pattern B — dst (v8) fed back as the index operand.
// Tests result-to-index turnaround latency, the relevant number for
// dependent LUT chains (e.g. quantization decode -> next-step indexing).
###############################
#void asimd_tbl_4table_v_u8_latency(int64_t);
    PROLOGUE asimd_tbl_4table_v_u8_latency
    preserve_caller_vec
    INIT
    movi v4.16b, #0
.asimd.tbl.4table.v.u8.lat.L1:
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    subs x0, x0, #1
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbl v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    bne .asimd.tbl.4table.v.u8.lat.L1
    restore_caller_vec
    ret

###############################
#void asimd_tbx_4table_v_u8_latency(int64_t);
    PROLOGUE asimd_tbx_4table_v_u8_latency
    preserve_caller_vec
    INIT
    movi v4.16b, #0
.asimd.tbx.4table.v.u8.lat.L1:
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    subs x0, x0, #1
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    tbx v8.16b, {v0.16b, v1.16b, v2.16b, v3.16b}, v8.16b
    bne .asimd.tbx.4table.v.u8.lat.L1
    restore_caller_vec
    ret
