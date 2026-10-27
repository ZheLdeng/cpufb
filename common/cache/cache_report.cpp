#include "cache/cache_report.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

#include "cache/associativity_probe.hpp"
#include "cache/cache_topology.hpp"
#include "cache/prefetch_probe.hpp"
#include "sys/thread_pool.hpp"
#include "table.hpp"

namespace cpufb {

namespace {

const int kFallbackL1CacheKiB = 64;
const int kFallbackL2CacheKiB = 1024;

int kib_of(uint64_t bytes)
{
    const uint64_t kib = bytes / 1024;
    return bytes >= 1024 &&
            kib <= static_cast<uint64_t>(std::numeric_limits<int>::max())
        ? static_cast<int>(kib)
        : 0;
}

std::string kib_text(int kib)
{
    return kib > 0 ? std::to_string(kib) + " KiB" : "-";
}

std::string count_text(int value)
{
    return value > 0 ? std::to_string(value) : "-";
}

std::string bytes_text(int bytes)
{
    return bytes > 0 ? std::to_string(bytes) + " B" : "-";
}

std::string join_note(const std::string &note, const std::string &more)
{
    if (more.empty()) return note;
    return note.empty() ? more : note + "; " + more;
}

// The agreement verdict, with the note explaining a withheld capacity
// replacing it (there is nothing to compare then) and the capacity's source
// in front.
std::string with_agreement(const std::string &source, double reported,
    int measured, double tolerance, const std::string &note)
{
    std::string verdict = describe_probe_agreement(reported, measured, tolerance);
    if (!note.empty()) verdict = measured <= 0 ? note : verdict + "; " + note;
    return source.empty() ? verdict : source + "; " + verdict;
}

// Capacities and notes from the curve.  Every level the curve resolved is
// reported; the note says when its boundary was not sharp, and the doubt
// about a prefetched curve goes on every level.
void assign_curve(CacheData &data)
{
    const CacheCurveResult &curve = data.curve;
    data.test_L1 = data.test_L2 = data.test_L3 = 0;
    data.test_L1_note.clear();
    data.test_L2_note.clear();
    data.test_L3_note.clear();
    const std::string doubt = describe_prefetch_doubt(curve);
    data.curve_trusted = doubt.empty();
    std::string deeper_steps;
    for (const CacheLevelEstimate &level : curve.levels) {
        const int size_kb = static_cast<int>(level.capacity_bytes / 1024);
        const std::string note = join_note(describe_transition(level), doubt);
        if (level.level == "L1") {
            data.test_L1 = size_kb;
            data.test_L1_note = note;
        } else if (level.level == "L2") {
            data.test_L2 = size_kb;
            data.test_L2_note = note;
        } else {
            // The L3 row is the deepest level the curve resolved, not its
            // third: a Kunpeng 920 resolves a step inside its 32 MiB L3 and
            // so reports four, and naming the third one L3 made the row read
            // "DISAGREES with OS, 0.12x" about a curve that had found the
            // 32 MiB exactly.  Anything between L2 and the deepest is named
            // in the note instead.
            if (data.test_L3 > 0)
                deeper_steps += (deeper_steps.empty() ? "" : ", ") +
                    format_cache_capacity(
                        static_cast<uint64_t>(data.test_L3) * 1024);
            data.test_L3 = size_kb;
            data.test_L3_note = note;
        }
    }
    if (!deeper_steps.empty())
        data.test_L3_note = join_note(data.test_L3_note,
            "the curve also stepped at " + deeper_steps + " below this level");
    data.memory_latency_ns = curve.memory_latency_ns;
    data.hierarchy_complete = curve.reached_memory;
}

} // namespace

void read_reported_cache(CacheData &data, int cpu)
{
    const CacheLevelInfo l1 = detect_data_cache_level(cpu, 1);
    const CacheLevelInfo l2 = detect_data_cache_level(cpu, 2);
    if (kib_of(l1.bytes) > 0) {
        data.theory_L1 = kib_of(l1.bytes);
        data.theory_L1_source = l1.source;
    }
    if (kib_of(l2.bytes) > 0) {
        data.theory_L2 = kib_of(l2.bytes);
        data.theory_L2_source = l2.source;
    }
    data.theory_way = l1.ways;
    // Left at 0 when the OS exposes nothing (Android): writing the 64-byte
    // default into it would present an assumption as a reported value.
    data.theory_cacheline = l1.line_bytes;
    data.theory_l2_way = l2.ways;
    data.theory_l2_line = l2.line_bytes;
}

void prepare_load_capacities(CacheData &data, int cpu)
{
    read_reported_cache(data, cpu);
    if (data.theory_L1 <= 0 && data.test_L1 <= 0) {
        data.theory_L1 = kFallbackL1CacheKiB;
        data.theory_L1_source = "conservative fallback";
    }
    if (data.theory_L2 <= 0 && data.test_L2 <= 0) {
        data.theory_L2 = kFallbackL2CacheKiB;
        data.theory_L2_source = "conservative fallback";
    }
}

void measure_cache_report(
    CacheData &data, int cpu, const CacheProbeKernels &kernels)
{
    if (!pin_current_thread(cpu))
        std::cerr << "Warning: cache probes could not bind to CPU " << cpu
                  << std::endl;
    // The curve first: the line probe sizes its eviction buffer from the
    // deepest level it finds, and the set-conflict probes step by the line.
    data.curve = measure_cache_curve(kernels.chase,
        effective_cacheline_size(data.theory_cacheline, data.test_cacheline,
            kernels.fallback_line_bytes),
        kernels.curve_max_bytes);
    assign_curve(data);
    debug_print_cache_curve(data.curve);

    data.test_cacheline = probe_cacheline_size(data.theory_cacheline,
        kernels.flush, kernels.fence,
        1024 *
            static_cast<size_t>(
                std::max({data.test_L1, data.test_L2, data.test_L3})));
    const int line = effective_cacheline_size(
        data.theory_cacheline, data.test_cacheline, kernels.fallback_line_bytes);
    // The set-conflict probes take no reported geometry, so their result is
    // an independent measurement rather than a confirmation of the OS, and
    // ways x bytes per way is an L1 capacity a prefetcher cannot stretch the
    // way it stretches the latency curve's plateau.
    data.test_way = probe_l1_associativity(line);
    data.test_way_bytes = probe_l1_way_bytes(line, data.test_way);
    data.test_line_from_sets =
        probe_l1_line_from_sets(data.test_way, data.test_way_bytes);
    data.test_l2_way = probe_l2_associativity(line, data.test_way);
    data.test_l2_line = probe_l2_line_from_sets(data.test_l2_way);
}

void append_cache_report_rows(
    Table &table, const CacheData &data, int cpu, double fallback_clock_ghz)
{
    std::vector<std::string> row(table.getCol());
    auto add = [&](const std::string &item, const std::string &reported,
                   const std::string &measured, const std::string &verdict) {
        row[0] = item;
        row[1] = reported;
        row[2] = measured;
        row[5] = verdict;
        table.addOneItem(row);
    };

    add("L1 data cache capacity", kib_text(data.theory_L1),
        kib_text(data.test_L1),
        with_agreement(data.theory_L1_source, data.theory_L1, data.test_L1,
            1.3,
            join_note(data.test_L1_note,
                describe_l1_curve_check(
                    data.test_L1, data.test_way, data.test_way_bytes))));
    add("L1 capacity from set conflicts", kib_text(data.theory_L1),
        data.test_way > 0 && data.test_way_bytes > 0
            ? format_cache_capacity(
                  static_cast<uint64_t>(data.test_way) * data.test_way_bytes)
            : "-",
        describe_l1_geometry(static_cast<uint64_t>(data.theory_L1) * 1024,
            data.test_way, data.test_way_bytes));
    add("L2/unified cache capacity", kib_text(data.theory_L2),
        kib_text(data.test_L2),
        with_agreement(data.theory_L2_source, data.theory_L2, data.test_L2,
            1.3, data.test_L2_note));
    // Always shown: "none" is a result too, and it is how a machine without
    // an L3 is told apart from one whose L3 the OS merely does not report.
    const CacheLevelInfo l3 = detect_data_cache_level(cpu, 3);
    add("L3/unified cache capacity",
        l3.bytes > 0 ? format_cache_capacity(l3.bytes) : "-",
        format_probed_l3(data.test_L3, data.hierarchy_complete),
        data.test_L3 > 0
            ? with_agreement("", l3.bytes / 1024.0, data.test_L3, 1.3,
                  join_note(data.test_L3_note,
                      describe_shared_level(l3, data.test_L3)))
            : l3.source);
    add("L1 ways of associativity", count_text(data.theory_way),
        count_text(data.test_way),
        with_agreement("", data.theory_way, data.test_way, 1.0, ""));
    add("L2 ways of associativity", count_text(data.theory_l2_way),
        count_text(data.test_l2_way),
        data.test_l2_way > 0
            ? with_agreement("", data.theory_l2_way, data.test_l2_way, 1.0, "")
            : describe_l2_unmeasured(data.test_l2_way));
    add("L1 cacheline size", bytes_text(data.theory_cacheline),
        bytes_text(data.test_cacheline),
        with_agreement("", data.theory_cacheline, data.test_cacheline, 1.0,
            describe_line_cross_check(
                data.test_cacheline, data.test_line_from_sets)));
    add("L2 cacheline size", bytes_text(data.theory_l2_line),
        bytes_text(data.test_l2_line),
        join_note(data.test_l2_line > 0
                ? describe_probe_agreement(
                      data.theory_l2_line, data.test_l2_line, 1.0) +
                    "; from set indexing"
                : describe_l2_unmeasured(data.test_l2_line),
            describe_deeper_line_sizes(cpu, data.theory_l2_line)));

    // The dependent-load sweep and the per-level estimates, in ns and in
    // cycles (PMU, else the frequency table's clock), then the prefetcher
    // reach, stream restart cost and software prefetch distance on the same
    // core, sized from the curve's levels.
    print_cache_curve_tables(data.curve, fallback_clock_ghz);
    append_prefetch_probe_rows(table, cpu,
        effective_cacheline_size(data.theory_cacheline, data.test_cacheline, 64),
        data.curve, fallback_clock_ghz);
}

uint64_t cache_report_last_level_bytes(const CacheData &data)
{
    return data.curve_trusted && data.test_L3 > 0
        ? static_cast<uint64_t>(data.test_L3) * 1024
        : 0;
}

} // namespace cpufb
