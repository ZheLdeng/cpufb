#!/usr/bin/env bash
set -euo pipefail

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(uname -s)" != "Linux" ||
      ( "$(uname -m)" != "aarch64" && "$(uname -m)" != "arm64" ) ]]; then
    echo "error: this regression test must run on a Linux ARM64 host" >&2
    exit 1
fi
if [[ $# -gt 1 ]]; then
    echo "usage: $0 [build-root]" >&2
    exit 1
fi

if [[ $# -eq 1 ]]; then
    build_root="$1"
    mkdir -p "${build_root}"
else
    build_root="$(mktemp -d "${TMPDIR:-/tmp}/cpufb-arm64-overrides.XXXXXX")"
fi

if [[ -n "${CPUFB_TEST_CORE:-}" ]]; then
    test_core="${CPUFB_TEST_CORE}"
else
    allowed_cpus="$(awk '/^Cpus_allowed_list:/ { print $2 }' /proc/self/status)"
    first_range="${allowed_cpus%%,*}"
    test_core="${first_range%%-*}"
fi

cpu_features="$(
    awk -F: '/^Features[[:space:]]*:/ { print $2; exit }' /proc/cpuinfo
)"
if [[ -z "${cpu_features}" ]]; then
    echo "error: could not read ARM64 Features from /proc/cpuinfo" >&2
    exit 1
fi

has_cpu_feature()
{
    local feature="$1"
    [[ " ${cpu_features} " == *" ${feature} "* ]]
}

contains_word()
{
    local words="$1"
    local expected="$2"
    [[ " ${words} " == *" ${expected} "* ]]
}

assert_compiled_features()
{
    local variant="$1"
    local actual_list="$2"
    shift 2
    local -a expected=("$@")
    local actual_words="${actual_list//;/ }"
    local -a actual=()
    local token

    read -r -a actual <<<"${actual_words}"
    if [[ ${#actual[@]} -ne ${#expected[@]} ]]; then
        echo "error: ${variant} compiled feature count mismatch" >&2
        echo "  actual:   ${actual_words}" >&2
        echo "  expected: ${expected[*]}" >&2
        exit 1
    fi
    for token in "${expected[@]}"; do
        if ! contains_word "${actual_words}" "${token}"; then
            echo "error: ${variant} omitted dependency ${token}" >&2
            exit 1
        fi
    done
    for token in "${actual[@]}"; do
        if ! contains_word "${expected[*]}" "${token}"; then
            echo "error: ${variant} unexpectedly compiled ${token}" >&2
            exit 1
        fi
    done
    echo "PASS ${variant}: compiled feature closure = ${actual_words}"
}

symbol_exists()
{
    local binary="$1"
    local symbol="$2"
    nm --defined-only "${binary}" |
        awk -v target="${symbol}" '$NF == target { found = 1 } END { exit !found }'
}

require_symbol()
{
    local variant="$1"
    local binary="$2"
    local symbol="$3"
    if ! symbol_exists "${binary}" "${symbol}"; then
        echo "error: ${variant} did not link required symbol ${symbol}" >&2
        exit 1
    fi
}

reject_symbol()
{
    local variant="$1"
    local binary="$2"
    local symbol="$3"
    if symbol_exists "${binary}" "${symbol}"; then
        echo "error: ${variant} unexpectedly linked symbol ${symbol}" >&2
        exit 1
    fi
}

runtime_token_expected()
{
    local token="$1"
    case "${token}" in
        _ASIMD_|_LDP_|_ISSUE_)
            has_cpu_feature asimd
            ;;
        _SVE_)
            has_cpu_feature sve
            ;;
        _SVE_BF16_)
            has_cpu_feature sve && has_cpu_feature svebf16
            ;;
        _SVE2_)
            has_cpu_feature sve && has_cpu_feature sve2
            ;;
        _SME_)
            has_cpu_feature sme
            ;;
        _SME2_)
            has_cpu_feature sme && has_cpu_feature sme2
            ;;
        *)
            return 1
            ;;
    esac
}

validate_runtime_tokens()
{
    local variant="$1"
    local compiled_list="$2"
    local list_output="$3"
    local runtime_line runtime_words token

    runtime_line="$(
        grep -F "Runtime ARM64 ISA:" <<<"${list_output}" | head -n 1
    )"
    if [[ -z "${runtime_line}" ]]; then
        echo "error: ${variant} did not print its runtime ISA list" >&2
        exit 1
    fi
    runtime_words="${runtime_line#*Runtime ARM64 ISA:}"

    for token in ${compiled_list//;/ }; do
        if runtime_token_expected "${token}"; then
            if ! contains_word "${runtime_words}" "${token}"; then
                echo "error: ${variant} filtered runnable token ${token}" >&2
                exit 1
            fi
        elif contains_word "${runtime_words}" "${token}"; then
            echo "error: ${variant} exposed unavailable token ${token}" >&2
            exit 1
        fi
    done
    echo "PASS ${variant}: runtime ISA filtering =${runtime_words}"
}

run_sweep()
{
    local variant="$1"
    local binary="$2"
    local instruction="$3"
    local csv="$4"

    "${binary}" "--thread_pool=[${test_core}]" \
        "--sweep-instruction=${instruction}" \
        --save="${csv}" --save-format=csv
    if ! awk -F, '
        $1 == "instruction_sweep" && $2 == "1" && ($4 + 0) > 0 {
            found = 1
        }
        END { if (!found) exit 1 }
    ' "${csv}"; then
        echo "error: ${variant} sweep output is missing or non-positive" >&2
        exit 1
    fi
    echo "PASS ${variant}: executed ${instruction}"
}

configure_variant()
{
    local variant="$1"
    local override="$2"
    shift 2
    local build_dir="${build_root}/${variant}"
    local configure_output feature_line compiled_list

    mkdir -p "${build_dir}"
    echo ">> configure ${variant}: ${override}"
    if ! configure_output="$(
        cmake -S "${source_dir}" -B "${build_dir}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_TESTING=ON \
            "-DCPUFB_SIMD_FEATURES_OVERRIDE=${override}" 2>&1
    )"; then
        printf '%s\n' "${configure_output}" >&2
        exit 1
    fi
    printf '%s\n' "${configure_output}"
    printf '%s\n' "${configure_output}" >"${build_dir}/configure.log"

    feature_line="$(
        grep -F "cpufb: compiled ARM64 kernels:" \
            <<<"${configure_output}" | tail -n 1
    )"
    if [[ -z "${feature_line}" ]]; then
        echo "error: ${variant} configure output omitted compiled kernels" >&2
        exit 1
    fi
    compiled_list="${feature_line#*cpufb: compiled ARM64 kernels: }"
    assert_compiled_features "${variant}" "${compiled_list}" "$@"
    printf '%s\n' "${compiled_list}" >"${build_dir}/compiled-features.txt"
}

build_and_run_variant()
{
    local variant="$1"
    local required_symbol="$2"
    local rejected_symbol="$3"
    local target_token="$4"
    local target_instruction="$5"
    local build_dir="${build_root}/${variant}"
    local binary="${build_dir}/cpufb"
    local compiled_list list_output memory_output

    echo ">> build ${variant}"
    cmake --build "${build_dir}" --parallel
    compiled_list="$(<"${build_dir}/compiled-features.txt")"

    require_symbol "${variant}" "${binary}" asimd_fmla_vv_f32f32f32
    require_symbol "${variant}" "${binary}" load_ldp_kernel
    require_symbol "${variant}" "${binary}" multiple_issue
    if [[ -n "${required_symbol}" ]]; then
        require_symbol "${variant}" "${binary}" "${required_symbol}"
    fi
    if [[ -n "${rejected_symbol}" ]]; then
        reject_symbol "${variant}" "${binary}" "${rejected_symbol}"
    fi

    list_output="$("${binary}" --list-instructions 2>&1)"
    validate_runtime_tokens "${variant}" "${compiled_list}" "${list_output}"

    ctest --test-dir "${build_dir}" --output-on-failure \
        -R 'cpufb_cli_(list|invalid_filters|memory_bandwidth)$'

    memory_output="$(
        "${binary}" "--thread_pool=[${test_core}]" \
            --memory-bandwidth --memory-size-mib=4 --memory-repetitions=1 2>&1
    )"
    if contains_word "${compiled_list//;/ }" "_SVE_" &&
       has_cpu_feature sve; then
        grep -Fq "sve-ld1h" <<<"${memory_output}" || {
            echo "error: ${variant} did not select its SVE memory kernel" >&2
            exit 1
        }
    else
        grep -Fq "neon-ld1h" <<<"${memory_output}" || {
            echo "error: ${variant} did not select its NEON memory kernel" >&2
            exit 1
        }
    fi

    if [[ -n "${target_token}" ]] &&
       runtime_token_expected "${target_token}"; then
        if ! contains_word "${list_output}" "${target_instruction}"; then
            echo "error: ${variant} omitted runnable instruction ${target_instruction}" >&2
            exit 1
        fi
        run_sweep "${variant}" "${binary}" "${target_instruction}" \
            "${build_dir}/sweep.csv"
    else
        if [[ -n "${target_token}" ]] &&
           contains_word "${list_output}" "${target_instruction}"; then
            echo "error: ${variant} exposed unavailable instruction ${target_instruction}" >&2
            exit 1
        fi
        run_sweep "${variant}" "${binary}" "fmla.vv(f32,f32,f32)" \
            "${build_dir}/baseline-sweep.csv"
        if [[ -n "${target_token}" ]]; then
            echo "PASS ${variant}: unavailable ${target_token} was filtered"
        fi
    fi
}

compiler_accepts_sme2()
{
    local build_dir="${build_root}/sme2"
    local compiler
    compiler="$(
        sed -n 's/^CMAKE_C_COMPILER:FILEPATH=//p' \
            "${build_dir}/CMakeCache.txt" | head -n 1
    )"
    [[ -n "${compiler}" ]] || return 1
    "${compiler}" -march=armv9-a+sme2 -x c -c /dev/null \
        -o "${build_dir}/sme2-flag-probe.o" >/dev/null 2>&1
}

echo "ARM64 SIMD override regression build root: ${build_root}"
echo "runtime test core: ${test_core}"
echo "CPU features:${cpu_features}"

configure_variant baseline _ASIMD_ \
    _ASIMD_ _LDP_ _ISSUE_
build_and_run_variant baseline "" load_sve_vector_bytes \
    _ASIMD_ "fmla.vv(f32,f32,f32)"

configure_variant sve_bf16 _SVE_BF16_ \
    _SVE_BF16_ _ASIMD_ _LDP_ _ISSUE_ _SVE_
build_and_run_variant sve_bf16 sve_bfmmla_f32bf16bf16 \
    sve2_sqrdmlah_vv_s16s16s16 \
    _SVE_BF16_ "sve_bfmmla(f32,bf16,bf16)"

configure_variant sve2 _SVE2_ \
    _SVE2_ _ASIMD_ _LDP_ _ISSUE_ _SVE_
build_and_run_variant sve2 sve2_sqrdmlah_vv_s16s16s16 \
    sve_bfmmla_f32bf16bf16 \
    _SVE2_ "sve2_sqrdmlah.vv(s16,s16,s16)"

configure_variant sme2 _SME2_ \
    _SME2_ _ASIMD_ _LDP_ _ISSUE_ _SME_
if compiler_accepts_sme2; then
    build_and_run_variant sme2 sme2_fmla4_mvv_f32f32f32 \
        sve_bfmmla_f32bf16bf16 \
        _SME2_ "sme2_fmla4.mvv(f32,f32,f32)"
else
    echo "SKIP sme2 build/run: configured compiler does not accept +sme2"
fi

echo "all supported ARM64 SIMD override builds passed"
