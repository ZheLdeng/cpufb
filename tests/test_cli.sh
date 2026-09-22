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

# ctest sets TMPDIR to the build tree; a direct invocation uses the binary's
# directory rather than /tmp, which a shared cluster may forbid.
test_tmp="$(mktemp -d "${TMPDIR:-$(dirname "${binary}")}/cpufb-cli.XXXXXX")"
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

require_single_migration_row()
{
    local csv_file="$1"
    awk -F',' '
        $1 == "system" && $2 == "Core Migration" {
            rows++
            if ($3 == "") exit 2
        }
        END { if (rows != 1) exit 3 }
    ' "${csv_file}" || fail "CSV output must contain one explicit migration status"
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
        for invalid_pool in '[1,]' '[2-1]' '[1x]' '1' \
            '[999999999999999999999999]' '[0-2147483647]'; do
            expect_failure "--thread_pool must use syntax" \
                "${binary}" "--thread_pool=${invalid_pool}" --include-test=freq
        done
        expect_failure "unknown test category 'not_a_test'" \
            "${binary}" "${thread_arg}" --include-test=not_a_test
        expect_failure "unavailable ISA category 'not_an_isa'" \
            "${binary}" "${thread_arg}" --exclude-isa=not_an_isa
        # An explicit --mode=all discards --include-test, but a typo in the
        # discarded filter must still be reported.
        expect_failure "unknown test category 'not_a_test'" \
            "${binary}" "${thread_arg}" --mode=all --include-test=not_a_test
        # A misspelled option used to be ignored and ran the full suite.
        expect_failure "unknown option '--include_test=freq'" \
            "${binary}" "${thread_arg}" --include_test=freq
        for invalid_number in -1 abc 99999999999; do
            expect_failure "--idle_time must be a non-negative integer" \
                "${binary}" "${thread_arg}" --include-test=freq \
                "--idle_time=${invalid_number}"
        done
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
        if [[ "$(grep -Ec '^\[[^]]+\]$' "${txt_output}")" -ne 2 ]]; then
            fail "filtered TXT output must contain system and compute sections"
        fi
        if ! grep -Fxq "[system]" "${txt_output}"; then
            fail "filtered TXT output omitted the system section"
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

    mode_all_override)
        txt_output="${test_tmp}/mode-all.txt"
        run_success "${binary}" "${thread_arg}" \
            --mode=all --include-test=compute \
            --exclude-test=compute,load,cache,multi_issue \
            --save="${txt_output}" >/dev/null
        if ! grep -Fxq "[freq]" "${txt_output}"; then
            fail "explicit --mode=all did not restore the frequency category"
        fi
        ;;

    csv_output)
        csv_output="${test_tmp}/frequency.csv"
        command_output="$(run_success "${binary}" "${thread_arg}" --include-test=freq \
            --save="${csv_output}")"
        require_contains "${command_output}" "Saved csv output:"
        require_contains "${command_output}" "System Information:"
        if [[ ! -s "${csv_output}" ]]; then
            fail "CSV output was not created"
        fi
        require_single_migration_row "${csv_output}"
        awk -F',' -v expected_core="${test_core}" -v arch="${arch}" '
            $1 == "section" && $2 == "Item" && $3 == "Value" {
                system_header = 1
                next
            }
            $1 == "system" {
                system_rows++
                # Values may be quoted because they contain commas, e.g.
                # "HiSilicon (0x48), ARM part 0xd22"; count fields with the
                # quoted ones collapsed instead of trusting -F",".
                unquoted = $0
                gsub(/"[^"]*"/, "q", unquoted)
                if (!system_header || split(unquoted, parts, ",") != 4) exit 2
                if ($2 == "OS") os_seen = 1
                if ($2 == "Kernel") kernel_seen = 1
                if ($2 == "Compiler") compiler_seen = 1
                if ($2 == "CPU Model") cpu_seen = 1
                if ($2 == "Core Selection" && $3 == "[" expected_core "]") cores_seen = 1
                if ($2 == "Core Migration") migration_seen = 1
                if ($2 == "CPU Frequency") frequency_seen = 1
                if ($2 == "L1 Data/Unified Cache") l1_seen = 1
                if ($2 == "L2 Data/Unified Cache") l2_seen = 1
                if ($2 == "L3 Data/Unified Cache") l3_seen = 1
                next
            }
            $1 == "section" && $2 == "Core ID" {
                columns = NF
                frequency_header = 1
                next
            }
            $1 == "freq" {
                rows++
                if (!frequency_header || NF != columns || $2 == "") exit 3
                if (arch == "x64" && $2 != expected_core) exit 4
            }
            END {
                if (rows != 1 || system_rows < 9 || !os_seen || !kernel_seen ||
                    !compiler_seen || !cpu_seen || !cores_seen || !migration_seen ||
                    !frequency_seen || !l1_seen || !l2_seen || !l3_seen) exit 5
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
        require_single_migration_row "${sweep_output}"
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
        require_single_migration_row "${memory_output}"
        awk -F',' -v expected_core="${test_core}" '
            $1 == "section" && $2 == "Core ID" && $5 == "Median GB/s" {
                columns = NF
                header_seen = 1
                if ($7 != "Load IPC") exit 2
                next
            }
            $1 == "memory_bandwidth" {
                rows++
                if (!header_seen || NF != columns ||
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
            $1 == "section" && $2 == "Item" &&
                $5 == "Median Bandwidth" && $6 == "Workset" {
                header_seen = 1
                next
            }
            $1 == "cache" && $2 == "Memory sequential read bandwidth" {
                rows++
                if (!header_seen || $3 != expected_core ||
                    $5 + 0 <= 0 || $6 != "4 MiB") exit 3
            }
            END {
                if (rows != 1) exit 4
            }
        ' "${cache_output}" || fail "cache memory bandwidth CSV row is invalid"
        # The line-size and associativity probes fail silently: a buffer that
        # is too small or a criterion that is too strict just yields "-" with
        # "probe: not observed" (both happened on Apple M4).  A missing or
        # disagreeing value is therefore treated as a regression.  The probes
        # time single cache misses, so load on the test core can spoil one run
        # (seen once on a Kunpeng 920F straight after a 32-way build); a real
        # regression fails every attempt, so one retry separates the two.
        # Capacities are not asserted: a busy SMT sibling or a co-tenant
        # legitimately lowers them.
        probes_agree()
        {
            awk -F',' '
                $1 == "cache" && ($2 == "cacheline size" ||
                        $2 == "L1 ways of associativity") {
                    rows++
                    if ($4 == "" || $4 == "-" || $0 ~ /not observed/) exit 5
                    if ($0 ~ /DISAGREES/) exit 6
                }
                END {
                    if (rows != 2) exit 7
                }
            ' "${cache_output}"
        }
        # Capacities are not compared with the OS (a busy sibling lowers them),
        # but a value that is printed must be plausible: an L1 of at most
        # 512 KiB, smaller than the L2 when both are given.  A cpu7 on a
        # MediaTek MT6993 reported "640 KiB" for a 64 KiB L1 before the
        # estimator learnt to withhold a slope's crossing point.
        awk -F',' '
            function kib(text) { sub(/ *K[i]?B.*/, "", text); return text + 0 }
            $1 == "cache" && $2 ~ /^L1 (data )?cache (capacity|size)$/ &&
                $4 ~ /K/ { l1 = kib($4) }
            $1 == "cache" && $2 ~ /^L2/ && $2 ~ /cache/ && $4 ~ /K/ {
                l2 = kib($4)
            }
            END {
                if (l1 > 512) exit 8
                if (l1 > 0 && l2 > 0 && l1 >= l2) exit 9
            }
        ' "${cache_output}" || fail "implausible cache capacities: $(grep -E 'cache (capacity|size)' "${cache_output}" | tr '\n' ' ')"
        if ! probes_agree; then
            first_attempt="$(grep -E 'cacheline size|ways of associativity' \
                "${cache_output}" | tr '\n' ' ')"
            run_success "${binary}" "${thread_arg}" --include-test=cache \
                --memory-size-mib=4 --memory-repetitions=1 \
                --save="${cache_output}" >/dev/null
            probes_agree || fail "cache-line or associativity probe did not report a value that agrees with the OS, twice: ${first_attempt}| $(grep -E 'cacheline size|ways of associativity' "${cache_output}" | tr '\n' ' ')"
        fi
        ;;

    *)
        fail "unknown test case"
        ;;
esac

echo "PASS ${test_case} (${arch}, core ${test_core})"
