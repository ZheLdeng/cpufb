#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

if [[ $# -ne 3 ]]; then
    echo "usage: $0 TEST_CASE CPUFB_BINARY ARCH" >&2
    exit 2
fi

test_case="$1"
binary="$2"
arch="$3"

if [[ ! -x "${binary}" ]]; then
    echo "FAIL ${test_case}: cpufb binary is not executable: ${binary}" >&2
    exit 1
fi
if [[ "${arch}" != "x64" && "${arch}" != "arm64" ]]; then
    echo "FAIL ${test_case}: unsupported test architecture: ${arch}" >&2
    exit 1
fi

test_tmp="$(mktemp -d "${TMPDIR:-/tmp}/cpufb-cli.XXXXXX")"
trap 'rm -rf "${test_tmp}"' EXIT

if [[ -n "${CPUFB_TEST_CORE:-}" ]]; then
    test_core="${CPUFB_TEST_CORE}"
elif [[ -r /proc/self/status ]]; then
    allowed_cpus="$(awk '/^Cpus_allowed_list:/ { print $2 }' /proc/self/status)"
    first_range="${allowed_cpus%%,*}"
    test_core="${first_range%%-*}"
else
    test_core=0
fi
thread_arg="--thread_pool=[${test_core}]"

fail()
{
    echo "FAIL ${test_case}: $*" >&2
    exit 1
}

require_contains()
{
    local value="$1"
    local expected="$2"
    if ! grep -Fq -- "${expected}" <<<"${value}"; then
        fail "expected output to contain: ${expected}"
    fi
}

expect_failure()
{
    local expected="$1"
    shift
    local output status

    set +e
    output="$("$@" 2>&1)"
    status=$?
    set -e

    if [[ ${status} -eq 0 ]]; then
        fail "command unexpectedly succeeded: $*"
    fi
    require_contains "${output}" "${expected}"
}

run_success()
{
    local output status

    set +e
    output="$("$@" 2>&1)"
    status=$?
    set -e

    if [[ ${status} -ne 0 ]]; then
        printf '%s\n' "${output}" >&2
        fail "command failed with status ${status}: $*"
    fi
    printf '%s\n' "${output}"
}

instruction_rows="${test_tmp}/instructions.tsv"
instruction_output=""

load_instruction_rows()
{
    if [[ -s "${instruction_rows}" ]]; then
        return
    fi

    instruction_output="$("${binary}" --list-instructions 2>&1)"
    printf '%s\n' "${instruction_output}" | awk -F'|' '
        function trim(value) {
            sub(/^[[:space:]]+/, "", value)
            sub(/[[:space:]]+$/, "", value)
            return value
        }
        NF >= 4 {
            isa = trim($2)
            instruction = trim($3)
            metric = trim($4)
            if (isa != "" && isa != "Instruction Set" &&
                instruction != "" && metric != "") {
                print isa "\t" instruction "\t" metric
            }
        }
    ' >"${instruction_rows}"

    if [[ ! -s "${instruction_rows}" ]]; then
        fail "--list-instructions returned no selectable compute instructions"
    fi
}

select_unique_instruction()
{
    load_instruction_rows
    awk -F'\t' '
        {
            count[$2]++
            row[NR] = $0
            name[NR] = $2
        }
        END {
            for (i = 1; i <= NR; i++) {
                if (count[name[i]] == 1) {
                    print row[i]
                    exit
                }
            }
        }
    ' "${instruction_rows}"
}

select_smallest_isa()
{
    load_instruction_rows
    awk -F'\t' '
        !seen[$1]++ { order[++n] = $1 }
        { count[$1]++ }
        END {
            selected = order[1]
            for (i = 2; i <= n; i++) {
                if (count[order[i]] < count[selected]) selected = order[i]
            }
            print selected
        }
    ' "${instruction_rows}"
}

case "${test_case}" in
    list)
        categories="$("${binary}" --list-categories 2>&1)"
        require_contains "${categories}" "Test categories:"
        require_contains "${categories}" "ISA categories:"
        for category in compute load cache freq multi_issue; do
            if ! grep -Eq "^[[:space:]]+${category}$" <<<"${categories}"; then
                fail "--list-categories omitted ${category}"
            fi
        done

        load_instruction_rows
        require_contains "${instruction_output}" "Instruction Set"
        require_contains "${instruction_output}" "Core Computation"
        require_contains "${instruction_output}" "Metric"
        if grep -Fq "_latency" "${instruction_rows}"; then
            fail "--list-instructions exposed latency-only rows"
        fi
        ;;

    invalid_filters)
        expect_failure "unknown test category 'not_a_test'" \
            "${binary}" "${thread_arg}" --include-test=not_a_test
        expect_failure "unavailable ISA category 'not_an_isa'" \
            "${binary}" "${thread_arg}" --exclude-isa=not_an_isa
        ;;

    instruction_matching)
        load_instruction_rows
        expect_failure "no compute instruction matched '__cpufb_no_such_instruction__'" \
            "${binary}" "${thread_arg}" \
            --sweep-instruction=__cpufb_no_such_instruction__

        ambiguous_needle=""
        for candidate in f32 f64 s32 u32 f16 bf16 add mul dp; do
            candidate_matches="$(awk -F'\t' -v needle="${candidate}" '
                index(tolower($2), needle) != 0 { count++ }
                END { print count + 0 }
            ' "${instruction_rows}")"
            if [[ ${candidate_matches} -ge 2 ]]; then
                ambiguous_needle="${candidate}"
                break
            fi
        done
        if [[ -z "${ambiguous_needle}" ]]; then
            fail "could not find an ambiguous partial instruction name"
        fi
        expect_failure "matched multiple compute instructions" \
            "${binary}" "${thread_arg}" \
            "--sweep-instruction=${ambiguous_needle}"
        ;;

    filters_txt)
        filter_isa="$(select_smallest_isa)"
        if [[ -z "${filter_isa}" ]]; then
            fail "could not select an ISA for filter testing"
        fi
        txt_output="${test_tmp}/filtered.txt"
        command_output="$(run_success "${binary}" "${thread_arg}" \
            --include-test=compute,freq --exclude-test=freq \
            "--include-isa=${filter_isa}" --save="${txt_output}")"
        require_contains "${command_output}" "Saved txt output:"
        if [[ ! -s "${txt_output}" ]]; then
            fail "TXT output was not created"
        fi
        if [[ "$(grep -Ec '^\[[^]]+\]$' "${txt_output}")" -ne 1 ]]; then
            fail "filtered TXT output must contain exactly one section"
        fi
        if ! grep -Fxq "[compute]" "${txt_output}"; then
            fail "filtered TXT output omitted the compute section"
        fi
        if grep -Fq "[freq]" "${txt_output}"; then
            fail "excluded freq section was serialized"
        fi
        awk -F'\t' -v expected="${filter_isa}" '
            $1 == "Instruction Set" { in_table = 1; next }
            in_table && NF >= 3 {
                rows++
                if ($1 != expected) exit 2
            }
            END {
                if (rows == 0) exit 3
            }
        ' "${txt_output}" || fail "ISA filter output contained missing or unexpected rows"
        ;;

    csv_output)
        csv_output="${test_tmp}/frequency.csv"
        command_output="$(run_success "${binary}" "${thread_arg}" --include-test=freq \
            --save="${csv_output}")"
        require_contains "${command_output}" "Saved csv output:"
        if [[ ! -s "${csv_output}" ]]; then
            fail "CSV output was not created"
        fi
        awk -F',' -v expected_core="${test_core}" -v arch="${arch}" '
            NR == 1 {
                columns = NF
                if ($1 != "section" || $2 != "Core ID") exit 2
                next
            }
            {
                rows++
                if (NF != columns || $1 != "freq" || $2 == "") exit 3
                if (arch == "x64" && $2 != expected_core) exit 4
            }
            END {
                if (rows != 1) exit 5
            }
        ' "${csv_output}" || fail "CSV frequency output has an invalid structure"
        ;;

    sweep_output)
        selected="$(select_unique_instruction)"
        if [[ -z "${selected}" ]]; then
            fail "could not find an unambiguous instruction for sweep testing"
        fi
        selected_rest="${selected#*$'\t'}"
        selected_instruction="${selected_rest%%$'\t'*}"
        sweep_output="${test_tmp}/sweep.csv"
        command_output="$(run_success "${binary}" "${thread_arg}" \
            "--sweep-instruction=${selected_instruction}" \
            --save="${sweep_output}")"
        require_contains "${command_output}" "Cores"
        require_contains "${command_output}" "Speedup"
        require_contains "${command_output}" "Efficiency"
        if [[ ! -s "${sweep_output}" ]]; then
            fail "sweep CSV output was not created"
        fi
        awk -F',' -v expected_pool="[${test_core}]" '
            $1 == "section" && $2 == "Cores" {
                header_columns = NF
                header_seen = 1
                next
            }
            $1 == "instruction_sweep" {
                rows++
                if (!header_seen || NF != header_columns || $2 != "1" ||
                    $3 != expected_pool || $6 !~ /x$/ || $7 !~ /%$/) exit 2
            }
            END {
                if (!header_seen || rows != 1) exit 3
            }
        ' "${sweep_output}" || fail "sweep CSV output has an invalid structure"
        if [[ "${arch}" == "arm64" && "${selected_instruction}" == *,* ]]; then
            if ! grep -Fq "\"${selected_instruction}\"" "${sweep_output}"; then
                fail "ARM64 sweep metadata did not CSV-quote its instruction name"
            fi
        fi
        ;;

    memory_bandwidth)
        expect_failure "--memory-size-mib must be a positive integer" \
            "${binary}" "${thread_arg}" --memory-bandwidth --memory-size-mib=-1
        expect_failure "require --memory-bandwidth" \
            "${binary}" "${thread_arg}" --memory-size-mib=4
        memory_output="${test_tmp}/memory.csv"
        command_output="$(run_success "${binary}" "${thread_arg}" \
            --memory-bandwidth --memory-size-mib=4 --memory-repetitions=1 \
            --save="${memory_output}")"
        require_contains "${command_output}" "Single-core memory bandwidth"
        require_contains "${command_output}" "Median GB/s"
        require_contains "${command_output}" "Load IPC"
        require_contains "${command_output}" "Saved csv output:"
        awk -F',' -v expected_core="${test_core}" '
            NR == 1 {
                columns = NF
                if ($1 != "section" || $2 != "Core ID" ||
                    $5 != "Median GB/s" || $7 != "Load IPC") exit 2
                next
            }
            {
                rows++
                if (NF != columns || $1 != "memory_bandwidth" ||
                    $2 != expected_core || $3 != "4 MiB" ||
                    $5 + 0 <= 0) exit 3
            }
            END {
                if (rows != 1) exit 4
            }
        ' "${memory_output}" || fail "memory bandwidth CSV output has an invalid structure"
        command_output="$(run_success "${binary}" \
            "--thread_pool=[${test_core},${test_core}]" \
            --memory-bandwidth --memory-size-mib=4 --memory-repetitions=1)"
        require_contains "${command_output}" "Multi-core memory bandwidth"
        require_contains "${command_output}" "2 synchronized read streams"
        require_contains "${command_output}" "2 streams"
        ;;

    cache_bandwidth)
        cache_output="${test_tmp}/cache.csv"
        command_output="$(run_success "${binary}" "${thread_arg}" \
            --include-test=cache --memory-size-mib=4 --memory-repetitions=1 \
            --save="${cache_output}")"
        require_contains "${command_output}" "Memory sequential read bandwidth"
        require_contains "${command_output}" "Saved csv output:"
        if [[ ! -s "${cache_output}" ]]; then
            fail "cache CSV output was not created"
        fi
        awk -F',' -v expected_core="core ${test_core}" '
            NR == 1 {
                if ($1 != "section" || $2 != "Item" ||
                    $5 != "Median Bandwidth" || $6 != "Workset") exit 2
                next
            }
            $1 == "cache" && $2 == "Memory sequential read bandwidth" {
                rows++
                if ($3 != expected_core || $5 + 0 <= 0 || $6 != "4 MiB") exit 3
            }
            END {
                if (rows != 1) exit 4
            }
        ' "${cache_output}" || fail "cache memory bandwidth CSV row is invalid"
        ;;

    *)
        fail "unknown test case"
        ;;
esac

echo "PASS ${test_case} (${arch}, core ${test_core})"
