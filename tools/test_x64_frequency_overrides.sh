#!/usr/bin/env bash
set -euo pipefail

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "error: this regression test must run on an x86-64 host" >&2
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
    # Under the source tree, not /tmp: a shared cluster may only be written
    # inside the project directory.
    mkdir -p "${source_dir}/build"
    build_root="$(mktemp -d "${source_dir}/build/cpufb-x64-frequency.XXXXXX")"
fi

if [[ -n "${CPUFB_TEST_CORE:-}" ]]; then
    test_core="${CPUFB_TEST_CORE}"
else
    allowed_cpus="$(awk '/^Cpus_allowed_list:/ { print $2 }' /proc/self/status)"
    first_range="${allowed_cpus%%,*}"
    test_core="${first_range%%-*}"
fi

variants=(sse2 avx2 avx_vnni)
features=(_SSE2_ _AVX2_ _AVX_VNNI_)

echo "x86 frequency regression build root: ${build_root}"
echo "runtime test core: ${test_core}"

for i in "${!variants[@]}"; do
    variant="${variants[i]}"
    feature="${features[i]}"
    build_dir="${build_root}/${variant}"
    binary="${build_dir}/cpufb"
    csv="${build_dir}/freq.csv"

    echo ">> configure ${variant}: ${feature} (no FMA override)"
    cmake -S "${source_dir}" -B "${build_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCPUFB_SIMD_FEATURES_OVERRIDE=${feature}"
    cmake --build "${build_dir}" --parallel

    if nm --defined-only "${binary}" | grep -Eq \
        '[[:space:]]fma_f(32|64)f(32|64)f(32|64)$'; then
        echo "error: ${variant} unexpectedly linked an optional FMA kernel" >&2
        exit 1
    fi

    "${source_dir}/tools/validate_x64_accounting.sh" "${binary}"
    "${binary}" "--thread_pool=[${test_core}]" --include-test=freq \
        --save="${csv}" --save-format=csv

    awk -F, '
        $1 == "freq" && $2 ~ /^[0-9]+$/ {
            found = 1
            if (($4 + 0) <= 0 || ($5 + 0) <= 0 ||
                ($6 + 0) <= 0 || ($7 + 0) <= 0) exit 1
        }
        END { if (!found) exit 1 }
    ' "${csv}" || {
        echo "error: ${variant} frequency output is missing or non-positive" >&2
        exit 1
    }
    echo "PASS ${variant}: linked and ran frequency test without FMA"
done

echo "all reduced x86 frequency builds passed"
