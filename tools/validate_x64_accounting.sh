#!/usr/bin/env bash
set -euo pipefail

binary="${1:-build/native-release/cpufb}"

if [[ ! -x "${binary}" ]]; then
    echo "error: x86 cpufb binary not found or not executable: ${binary}" >&2
    exit 1
fi

for tool in nm objdump; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "error: required tool not found: ${tool}" >&2
        exit 1
    fi
done

symbol_table="$(nm -n --defined-only "${binary}")"
sized_symbol_table="$(nm -n -S --defined-only "${binary}")"
checked=0
skipped=0

check_count() {
    local symbol="$1"
    local mnemonic="$2"
    local expected="$3"
    local bounds start stop size count symbol_line
    local -a symbol_fields

    if ! grep -Eq "[[:space:]]T[[:space:]]${symbol}$" <<<"${symbol_table}"; then
        echo "SKIP ${symbol} (not built for this host)"
        skipped=$((skipped + 1))
        return
    fi

    symbol_line="$(awk -v target="${symbol}" '$NF == target { print; exit }' \
        <<<"${sized_symbol_table}")"
    read -ra symbol_fields <<<"${symbol_line}"
    start="${symbol_fields[0]}"
    size=""
    if [[ ${#symbol_fields[@]} -eq 4 ]]; then
        size="${symbol_fields[1]}"
    fi

    if [[ -n "${size}" && "${size}" != "0000000000000000" ]]; then
        stop="$(printf '%x' "$((16#${start} + 16#${size}))")"
    else
        # Most legacy assembly symbols have no ELF size. Preserve their local
        # hot-loop labels by stopping at the next global text symbol.
        bounds="$(awk -v target="${symbol}" '
            $2 == "T" && $3 == target { start = $1; found = 1; next }
            found && $2 == "T" { print start, $1; exit }
            END { if (found && start != "") print start }
        ' <<<"${symbol_table}" | head -n 1)"
        read -r start stop <<<"${bounds}"
    fi

    if [[ -n "${stop:-}" ]]; then
        count="$(objdump -d --no-show-raw-insn \
            --start-address="0x${start}" --stop-address="0x${stop}" \
            "${binary}" | grep -cw "${mnemonic}" || true)"
    else
        count="$(objdump -d --no-show-raw-insn \
            --start-address="0x${start}" "${binary}" | \
            grep -cw "${mnemonic}" || true)"
    fi

    if [[ "${count}" != "${expected}" ]]; then
        echo "FAIL ${symbol}: ${mnemonic} count=${count}, expected=${expected}" >&2
        exit 1
    fi

    echo "PASS ${symbol}: ${mnemonic} count=${count}"
    checked=$((checked + 1))
}

check_count cpufb_x64_frequency_fsu32 addps 16
check_count cpufb_x64_frequency_fsu64 addpd 16
check_count cpufb_x64_frequency_load movups 16
check_count sse_add_f32 addps 16
check_count sse_add_f32_latency addps 16
check_count sse2_add_f64 addpd 16
check_count sse2_add_f64_latency addpd 16
check_count avx_add_f32 vaddps 16
check_count avx_add_f32_latency vaddps 16
check_count avx_add_mul_f32f32_f32 vmulps 8
check_count avx_add_mul_f32f32_f32 vaddps 8
check_count avx_add_mul_f32f32_f32_latency vmulps 8
check_count avx_add_mul_f32f32_f32_latency vaddps 8
check_count avx_add_mul_f64f64_f64_latency vmulpd 8
check_count avx_add_mul_f64f64_f64_latency vaddpd 8
check_count fma_f32f32f32 vfmadd231ps 16
check_count fma_f32f32f32_latency vfmadd231ps 16
check_count avx2_add_s32 vpaddd 16
check_count avx2_add_s32_latency vpaddd 16
check_count avx_vnni_dp4a_s32u8s8 vpdpbusd 16
check_count avx_vnni_dp4a_s32u8s8_latency vpdpbusd 16
check_count avx_vnni_int8_dp4a_s32s8s8 vpdpbssd 16
check_count avx512f_fma_f32f32f32 vfmadd231ps 16
check_count avx512f_fma_f32f32f32_latency vfmadd231ps 16
check_count avx512_vnni_dp4a_s32u8s8 vpdpbusd 16
check_count avx512_vnni_dp4a_s32u8s8_latency vpdpbusd 16
check_count avx512_bf16_dp2a_f32bf16bf16 vdpbf16ps 16
check_count avx512_bf16_dp2a_f32bf16bf16_latency vdpbf16ps 16
check_count avx512_fp16_fma_f16f16f16 vfmadd231ph 16
check_count avx512_fp16_fma_f16f16f16_latency vfmadd231ph 16
check_count avx512_ifma_madd52_u64 vpmadd52luq 16
check_count avx512_ifma_madd52_u64_latency vpmadd52luq 16
check_count avx512_vbmi_permb_u8 vpermb 16
check_count avx512_vbmi_permb_u8_latency vpermb 16
check_count avx512_vpopcnt_s32 vpopcntd 16
check_count avx512_vpopcnt_s32_latency vpopcntd 16
check_count amx_int8_mm_s32s8s8 tdpbssd 4
check_count amx_int8_mm_s32s8s8_latency tdpbssd 4
check_count amx_bf16_mm_f32bf16bf16 tdpbf16ps 4
check_count amx_bf16_mm_f32bf16bf16_latency tdpbf16ps 4

echo "validated ${checked} kernels; skipped ${skipped} unavailable kernels"
