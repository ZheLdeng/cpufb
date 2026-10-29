#include "cache/prefetch_probe.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <numeric>
#include <random>

#include "cache/cache_topology.hpp"
#include "table.hpp"

namespace cpufb {

const PrefetchColumn kPrefetchColumnSpec[kPrefetchColumns] = {
    {PREFETCH_L2_WORKSET, false},
    {PREFETCH_MEMORY_WORKSET, false},
    {PREFETCH_MEMORY_WORKSET, true},
};

const char *prefetch_instruction_name(bool l2_target)
{
#if defined(__aarch64__)
    return l2_target ? "prfm pldl2keep" : "prfm pldl1keep";
#elif defined(__x86_64__)
    return l2_target ? "prefetcht1" : "prefetcht0";
#elif defined(__riscv)
    return l2_target ? "prefetch.r (Zicbop) for L2"
                     : "prefetch.r (Zicbop), nothing without it";
#else
    return l2_target ? "__builtin_prefetch(., 0, 2)" : "__builtin_prefetch(., 0, 3)";
#endif
}

namespace {

// 3 and 6 tell a stride the prefetcher does not follow from one that aliases
// the cache's set index (an Apple M4 hid nothing at stride 4 alone).
const size_t kStrides[] = {1, 2, 3, 4, 6, 8, 16, 32, 64};
const size_t kSegments[] = {4, 8, 16, 32, 64, 128, 256, 1024};
const size_t kDistances[] = {0, 1, 2, 4, 8, 16, 32, 64};
const size_t kMaxDistance = 64;
// The memory working set: no cache of a single core holds 64 MiB, and four
// times the deepest level the curve found when that is larger.
const uint64_t kMinimumMemoryWorkset = 64ULL << 20;
// Dependent loads per timed sample, spread over whole laps of the ring.
const size_t kStepsPerSample = 1u << 20;
const int kSamples = 3;
const double kFollowedShare = 0.5;    // of what stride 1 hides
const double kMinimumHidden = 0.2;    // below this stride 1 hid nothing
const double kDistanceSlack = 1.03;   // "within 3% of the best"
const double kEffectiveRatio = 0.9;   // best must beat alone by 10%

volatile int32_t chase_sink = 0;

double elapsed_ns(const timespec &start, const timespec &end)
{
    return static_cast<double>(end.tv_sec - start.tv_sec) * 1e9 +
        static_cast<double>(end.tv_nsec - start.tv_nsec);
}

struct Sample
{
    double ns = 0;
    double cycles = 0;
};

// Follows `steps` links from word 0.
__attribute__((noinline)) int32_t chase(const int32_t *buffer, size_t steps)
{
    int32_t next = 0;
    for (size_t i = 0; i < steps; ++i) next = buffer[next];
    return next;
}

// The same chase with the node `distance` steps ahead prefetched from the
// side array; distance 0 prefetches the line being loaded (the baseline).
template <int Locality>
__attribute__((noinline)) int32_t chase_prefetching(const int32_t *buffer,
    const int32_t *nodes, size_t count, size_t distance, size_t laps)
{
    int32_t next = 0;
    for (size_t lap = 0; lap < laps; ++lap) {
        for (size_t i = 0; i < count; ++i) {
            next = buffer[next];
            __builtin_prefetch(buffer + nodes[i + distance], 0, Locality);
        }
    }
    return next;
}

// Fastest of kSamples runs after one warm run; the cycles are those of the
// fastest run itself.
template <typename Run>
Sample time_best(SampleCycles &cycles, size_t steps, Run run)
{
    run();
    Sample best;
    for (int sample = 0; sample < kSamples; ++sample) {
        timespec start, end;
        if (cycles.available()) cycles.start();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        run();
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double counted = cycles.available() ? cycles.stop() : 0.0;
        const double ns = elapsed_ns(start, end) / steps;
        if (ns > 0.0 && (best.ns == 0.0 || ns < best.ns)) {
            best.ns = ns;
            best.cycles = counted > 0.0 ? counted / steps : 0.0;
        }
    }
    return best;
}

// Writes the ring that visits the lines in `order` into the buffer, starting
// at line 0, and fills `nodes` with the word index of every line in order
// plus kMaxDistance wrapped entries for the prefetch look-ahead.
void link_ring(int32_t *buffer, size_t stride_words, std::vector<size_t> order,
    std::vector<int32_t> &nodes)
{
    std::rotate(
        order.begin(), std::find(order.begin(), order.end(), 0), order.end());
    const size_t count = order.size();
    nodes.resize(count + kMaxDistance);
    for (size_t i = 0; i < count; ++i)
        nodes[i] = static_cast<int32_t>(order[i] * stride_words);
    for (size_t i = 0; i < kMaxDistance; ++i) nodes[count + i] = nodes[i % count];
    for (size_t i = 0; i < count; ++i)
        buffer[nodes[i]] = nodes[(i + 1) % count];
}

// Lines 0, s, 2s, ... then 1, 1+s, ...: every line once, stride s.
std::vector<size_t> stride_order(size_t line_count, size_t stride)
{
    std::vector<size_t> order;
    order.reserve(line_count);
    for (size_t phase = 0; phase < stride && phase < line_count; ++phase)
        for (size_t line = phase; line < line_count; line += stride)
            order.push_back(line);
    return order;
}

// Sequential segments of `segment` lines, the segments in random order.
std::vector<size_t> segment_order(
    size_t line_count, size_t segment, uint64_t seed)
{
    std::mt19937_64 random(seed);
    const size_t groups = (line_count + segment - 1) / segment;
    std::vector<size_t> group(groups);
    std::iota(group.begin(), group.end(), 0);
    std::shuffle(group.begin(), group.end(), random);
    std::vector<size_t> order;
    order.reserve(line_count);
    for (size_t g : group)
        for (size_t line = g * segment;
            line < std::min(line_count, (g + 1) * segment); ++line)
            order.push_back(line);
    return order;
}

size_t laps_for(size_t line_count)
{
    return std::max<size_t>(1, kStepsPerSample / line_count);
}

const char *workset_name(PrefetchWorksetKind workset)
{
    return workset == PREFETCH_L2_WORKSET ? "L2 workset" : "memory workset";
}

std::string format_ns(double ns)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.2f ns", ns);
    return text;
}

// "5.9 ns (15.3 cycles)" or "15.3 cycles" when the clock is the only source
// of cycles anyway, the ns stay.
std::string format_ns_cycles(double ns, double cycles, double clock_ghz)
{
    const std::string c = format_latency_cycles(ns, cycles, clock_ghz);
    return c == "-" ? format_ns(ns) : format_ns(ns) + " (" + c + " cycles)";
}

std::string format_percent(double share)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%.0f%%", share * 100.0);
    return text;
}

double distance_ns(const PrefetchProbeResult &result, size_t column, size_t distance)
{
    for (const PrefetchDistance &row : result.distances)
        if (row.distance == distance) return row.ns[column];
    return 0.0;
}

double distance_cycles(const PrefetchProbeResult &result, size_t column, size_t distance)
{
    for (const PrefetchDistance &row : result.distances)
        if (row.distance == distance) return row.cycles[column];
    return 0.0;
}

} // namespace

// ---------------------------------------------------------------------------
// Analysis
// ---------------------------------------------------------------------------

StrideReach stride_reach(
    const PrefetchProbeResult &result, PrefetchWorksetKind workset)
{
    StrideReach reach;
    const double random_ns = result.worksets[workset].random_ns;
    if (!(random_ns > 0.0) || result.strides.empty()) return reach;
    const double stride1 = result.strides.front().ns[workset];
    if (!(stride1 > 0.0)) return reach;
    reach.hidden_stride1 = std::max(0.0, 1.0 - stride1 / random_ns);
    if (reach.hidden_stride1 < kMinimumHidden) return reach;
    const double hidden1 = random_ns - stride1;
    for (const PrefetchStride &row : result.strides) {
        const double hidden = random_ns - row.ns[workset];
        if (!(row.ns[workset] > 0.0) || hidden < kFollowedShare * hidden1)
            break;
        reach.reach_lines = row.stride_lines;
    }
    return reach;
}

RestartCost fit_restart_cost(
    const std::vector<PrefetchSegment> &segments, double steady_ns)
{
    RestartCost cost;
    cost.steady_ns = steady_ns;
    if (!(steady_ns > 0.0)) return cost;
    // T(S) = T + C/S: least squares for C with x = 1/S, y = T(S) - T.
    double xy = 0.0, xx = 0.0;
    for (const PrefetchSegment &segment : segments) {
        if (segment.lines == 0 || !(segment.ns > 0.0)) continue;
        const double x = 1.0 / static_cast<double>(segment.lines);
        xy += x * (segment.ns - steady_ns);
        xx += x * x;
    }
    if (xx > 0.0) cost.cost_ns = std::max(0.0, xy / xx);
    for (const PrefetchSegment &segment : segments) {
        if (segment.lines == 0 || !(segment.ns > 0.0)) continue;
        if (cost.lines_for_ten_percent == 0 && segment.ns <= 1.1 * steady_ns)
            cost.lines_for_ten_percent = segment.lines;
        if (segment.lines >= cost.longest_lines) {
            cost.longest_lines = segment.lines;
            cost.longest_overhead = segment.ns / steady_ns - 1.0;
        }
    }
    return cost;
}

DistanceSummary summarize_distances(
    const PrefetchProbeResult &result, size_t column)
{
    DistanceSummary summary;
    if (column >= kPrefetchColumns) return summary;
    summary.alone_ns = distance_ns(result, column, 0);
    for (const PrefetchDistance &row : result.distances) {
        const double ns = row.ns[column];
        if (!(ns > 0.0)) continue;
        if (summary.best_ns == 0.0 || ns < summary.best_ns) {
            summary.best_ns = ns;
            summary.best_distance = row.distance;
        }
    }
    if (!(summary.alone_ns > 0.0) || !(summary.best_ns > 0.0)) return summary;
    for (const PrefetchDistance &row : result.distances) {
        if (row.ns[column] > 0.0 &&
            row.ns[column] <= kDistanceSlack * summary.best_ns) {
            summary.distance_needed = row.distance;
            break;
        }
    }
    summary.lines_in_flight = summary.alone_ns / summary.best_ns;
    summary.effective = summary.best_ns <= kEffectiveRatio * summary.alone_ns;
    return summary;
}

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------

PrefetchProbeResult measure_prefetch_probe(
    int line_bytes, uint64_t l1_bytes, uint64_t l2_bytes, uint64_t deepest_bytes)
{
    PrefetchProbeResult result;
    const size_t line = line_bytes > 0 ? static_cast<size_t>(line_bytes) : 64;
    const size_t stride_words = std::max<size_t>(1, line / sizeof(int32_t));
    const bool huge_pages = transparent_huge_pages_available();
    const size_t group_lines =
        ring_group_lines(line, huge_pages, result.translation_mode);

    uint64_t memory_bytes =
        std::max<uint64_t>(kMinimumMemoryWorkset, 4 * deepest_bytes);
    const uint64_t available = available_memory_bytes();
    while (available > 0 && memory_bytes > available / 4 &&
        memory_bytes > kMinimumMemoryWorkset)
        memory_bytes /= 2;
    if (available > 0 && memory_bytes > available / 4) memory_bytes = 0;
    // Nodes are 32-bit word indices.
    if (memory_bytes / sizeof(int32_t) > 0x7fffffffULL) memory_bytes = 0;
    // Geometric middle of (L1, L2]: far from both edges on a log scale.
    uint64_t l2_workset = l1_bytes > 0 && l2_bytes > l1_bytes
        ? static_cast<uint64_t>(std::sqrt(
              static_cast<double>(l1_bytes) * static_cast<double>(l2_bytes)))
        : 0;
    l2_workset -= l2_workset % 4096;
    const uint64_t region_bytes = std::max(memory_bytes, l2_workset);
    if (region_bytes < 2 * line) return result;
    MappedRegion region;
    if (!region.map(region_bytes, huge_pages)) return result;
    int32_t *buffer = static_cast<int32_t *>(region.base());
    result.worksets[PREFETCH_L2_WORKSET].bytes = l2_workset;
    result.worksets[PREFETCH_MEMORY_WORKSET].bytes = memory_bytes;

    SampleCycles cycles;
    std::vector<int32_t> nodes;
    const uint64_t seed = 0x5052454645544348ULL;
    auto lines_of = [&](int workset) {
        return static_cast<size_t>(result.worksets[workset].bytes / line);
    };
    auto measure_order = [&](const std::vector<size_t> &order) {
        link_ring(buffer, stride_words, order, nodes);
        const size_t steps = order.size() * laps_for(order.size());
        return time_best(
            cycles, steps, [&]() { chase_sink = chase(buffer, steps); });
    };

    for (int w = 0; w < 2; ++w) {
        if (result.worksets[w].bytes == 0) continue;
        const Sample random =
            measure_order(build_ring_order(lines_of(w), group_lines, seed + w));
        result.worksets[w].random_ns = random.ns;
        result.worksets[w].random_cycles = random.cycles;
    }

    for (size_t stride : kStrides) {
        PrefetchStride row;
        row.stride_lines = stride;
        for (int w = 0; w < 2; ++w) {
            if (result.worksets[w].bytes == 0 || lines_of(w) <= stride) continue;
            const Sample sample = measure_order(stride_order(lines_of(w), stride));
            row.ns[w] = sample.ns;
            row.cycles[w] = sample.cycles;
        }
        result.strides.push_back(row);
    }

    if (memory_bytes > 0) {
        const size_t count = lines_of(PREFETCH_MEMORY_WORKSET);
        for (size_t segment : kSegments) {
            if (segment >= count) break;
            PrefetchSegment row;
            row.lines = segment;
            const Sample sample =
                measure_order(segment_order(count, segment, seed ^ segment));
            row.ns = sample.ns;
            row.cycles = sample.cycles;
            result.segments.push_back(row);
        }
    }

    for (size_t distance : kDistances) {
        PrefetchDistance row;
        row.distance = distance;
        result.distances.push_back(row);
    }
    for (size_t column = 0; column < kPrefetchColumns; ++column) {
        const PrefetchColumn spec = kPrefetchColumnSpec[column];
        if (result.worksets[spec.workset].bytes == 0) continue;
        const size_t count = lines_of(spec.workset);
        link_ring(buffer, stride_words,
            build_ring_order(count, group_lines, seed + 7 + column), nodes);
        const size_t laps = laps_for(count);
        for (PrefetchDistance &row : result.distances) {
            const size_t distance = row.distance;
            if (distance == 0) {
                // The plain chase over the same random order.
                row.ns[column] = result.worksets[spec.workset].random_ns;
                row.cycles[column] = result.worksets[spec.workset].random_cycles;
                continue;
            }
            auto run = [&]() {
                chase_sink = spec.l2_target
                    ? chase_prefetching<2>(buffer, nodes.data(), count, distance, laps)
                    : chase_prefetching<3>(buffer, nodes.data(), count, distance, laps);
            };
            const Sample sample = time_best(cycles, count * laps, run);
            row.ns[column] = sample.ns;
            row.cycles[column] = sample.cycles;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

namespace {

std::string describe_stride_reach(
    const PrefetchProbeResult &result, double clock_ghz)
{
    (void)clock_ghz;
    std::string text;
    for (int w = 0; w < 2; ++w) {
        if (result.worksets[w].bytes == 0) continue;
        const StrideReach reach =
            stride_reach(result, static_cast<PrefetchWorksetKind>(w));
        if (!text.empty()) text += "; ";
        text += workset_name(static_cast<PrefetchWorksetKind>(w));
        if (reach.reach_lines == 0)
            text += ": no sequential prefetch observed (stride 1 hides " +
                format_percent(reach.hidden_stride1) +
                " of the random-order latency)";
        else
            text += ": follows strides up to " +
                std::to_string(reach.reach_lines) + " lines, stride 1 hides " +
                format_percent(reach.hidden_stride1) +
                " of the random-order latency";
    }
    return text.empty() ? "not measured" : text;
}

std::string describe_restart_cost(
    const PrefetchProbeResult &result, double clock_ghz)
{
    if (result.segments.empty() || result.strides.empty()) return "not measured";
    const double steady = result.strides.front().ns[PREFETCH_MEMORY_WORKSET];
    const RestartCost cost = fit_restart_cost(result.segments, steady);
    if (!(cost.steady_ns > 0.0)) return "not measured";
    // Cycles of the cost from the counted cycles of the steady stream.
    const double steady_cycles =
        result.strides.front().cycles[PREFETCH_MEMORY_WORKSET];
    const double cost_cycles = steady_cycles > 0.0
        ? cost.cost_ns * steady_cycles / cost.steady_ns
        : 0.0;
    char text[256];
    std::snprintf(text, sizeof(text),
        "%s per stream start, %.1f line-times at %s per line steady; ",
        format_ns_cycles(cost.cost_ns, cost_cycles, clock_ghz).c_str(),
        cost.cost_ns / cost.steady_ns,
        format_ns_cycles(cost.steady_ns, steady_cycles, clock_ghz).c_str());
    char tail[128];
    if (cost.lines_for_ten_percent > 0)
        std::snprintf(tail, sizeof(tail),
            "segments of %zu lines or more run within 10%% of it",
            cost.lines_for_ten_percent);
    else
        std::snprintf(tail, sizeof(tail),
            "even %zu-line segments run %.0f%% over it", cost.longest_lines,
            cost.longest_overhead * 100.0);
    return std::string(text) + tail;
}

std::string describe_distance_column(
    const PrefetchProbeResult &result, size_t column, double clock_ghz)
{
    const PrefetchColumn spec = kPrefetchColumnSpec[column];
    if (result.worksets[spec.workset].bytes == 0) return "";
    const DistanceSummary summary = summarize_distances(result, column);
    std::string text = workset_name(spec.workset);
    if (!(summary.alone_ns > 0.0)) return text + ": not measured";
    const std::string alone = format_ns_cycles(summary.alone_ns,
        distance_cycles(result, column, 0), clock_ghz);
    if (!summary.effective)
        return text + ": no measurable effect (best " +
            format_ns(summary.best_ns) + " per load against " + alone +
            " without prefetch)";
    char tail[192];
    std::snprintf(tail, sizeof(tail),
        ": %s per load from distance %zu on (%s alone, %.1f lines in flight)",
        format_ns_cycles(summary.best_ns,
            distance_cycles(result, column, summary.best_distance), clock_ghz)
            .c_str(),
        summary.distance_needed, alone.c_str(), summary.lines_in_flight);
    return text + tail;
}

void append_row(Table &table, const std::string &item, int cpu,
    const std::string &probe, const std::string &bandwidth,
    const std::string &workset, const std::string &measurement)
{
    std::vector<std::string> row(table.getCol());
    row[0] = item;
    row[1] = "core " + std::to_string(cpu);
    row[2] = probe;
    row[3] = bandwidth;
    row[4] = workset;
    row[5] = measurement;
    table.addOneItem(row);
}

std::string worksets_text(const PrefetchProbeResult &result)
{
    std::string text;
    for (int w = 0; w < 2; ++w) {
        if (result.worksets[w].bytes == 0) continue;
        if (!text.empty()) text += " / ";
        text += format_cache_capacity(result.worksets[w].bytes);
    }
    return text.empty() ? "-" : text;
}

} // namespace

void print_prefetch_probe_tables(
    const PrefetchProbeResult &result, double fallback_clock_ghz)
{
    bool counted = false;
    for (const PrefetchStride &row : result.strides)
        for (int w = 0; w < 2; ++w)
            if (row.cycles[w] > 0.0) counted = true;
    std::printf("Prefetch probe: L2 workset %s, memory workset %s, %s; "
                "cycles: %s\n",
        result.worksets[0].bytes ? format_cache_capacity(result.worksets[0].bytes).c_str() : "none",
        result.worksets[1].bytes ? format_cache_capacity(result.worksets[1].bytes).c_str() : "none",
        result.translation_mode.c_str(),
        counted ? "perf_event cycles"
                : fallback_clock_ghz > 0.0 ? "latency x frequency table clock"
                                           : "not available");

    auto cell = [&](double ns, double cycles) {
        return ns > 0.0 ? format_ns_cycles(ns, cycles, fallback_clock_ghz)
                        : std::string("-");
    };
    auto hidden = [&](int w, double ns) {
        const double random = result.worksets[w].random_ns;
        return ns > 0.0 && random > 0.0
            ? format_percent(std::max(0.0, 1.0 - ns / random))
            : std::string("-");
    };

    Table strides;
    strides.setColumnNum(5);
    std::vector<std::string> head = {"Dependent-load order",
        "L2 workset latency", "hidden", "Memory workset latency", "hidden"};
    strides.addOneItem(head);
    {
        std::vector<std::string> row = {"random order",
            cell(result.worksets[0].random_ns, result.worksets[0].random_cycles),
            "-",
            cell(result.worksets[1].random_ns, result.worksets[1].random_cycles),
            "-"};
        strides.addOneItem(row);
    }
    for (const PrefetchStride &s : result.strides) {
        std::vector<std::string> row = {
            "sequential, stride " + std::to_string(s.stride_lines) +
                (s.stride_lines == 1 ? " line" : " lines"),
            cell(s.ns[0], s.cycles[0]), hidden(0, s.ns[0]),
            cell(s.ns[1], s.cycles[1]), hidden(1, s.ns[1])};
        strides.addOneItem(row);
    }
    strides.print();

    if (!result.segments.empty()) {
        const double steady = result.strides.front().ns[1];
        Table segments;
        segments.setColumnNum(3);
        std::vector<std::string> segment_head = {
            "Sequential segments (lines, memory workset)", "Latency per load",
            "over the endless stream"};
        segments.addOneItem(segment_head);
        for (const PrefetchSegment &s : result.segments) {
            char over[32];
            std::snprintf(over, sizeof(over), "%+.0f%%",
                steady > 0.0 ? (s.ns / steady - 1.0) * 100.0 : 0.0);
            std::vector<std::string> row = {
                std::to_string(s.lines), cell(s.ns, s.cycles), over};
            segments.addOneItem(row);
        }
        segments.print();
    }

    Table distances;
    distances.setColumnNum(1 + kPrefetchColumns);
    std::vector<std::string> distance_head = {"Prefetch distance (lines)"};
    for (size_t c = 0; c < kPrefetchColumns; ++c)
        distance_head.push_back(
            std::string(workset_name(kPrefetchColumnSpec[c].workset)) + ", " +
            prefetch_instruction_name(kPrefetchColumnSpec[c].l2_target));
    distances.addOneItem(distance_head);
    for (const PrefetchDistance &d : result.distances) {
        std::vector<std::string> row = {
            d.distance == 0 ? "0 (no prefetch)" : std::to_string(d.distance)};
        for (size_t c = 0; c < kPrefetchColumns; ++c)
            row.push_back(cell(d.ns[c], d.cycles[c]));
        distances.addOneItem(row);
    }
    distances.print();
}

void append_prefetch_probe_rows(Table &table, int cpu, int line_bytes,
    const CacheCurveResult &curve, double fallback_clock_ghz)
{
    uint64_t l1 = 0, l2 = 0, deepest = 0;
    for (const CacheLevelEstimate &level : curve.levels) {
        if (level.level == "L1") l1 = level.capacity_bytes;
        if (level.level == "L2") l2 = level.capacity_bytes;
        deepest = std::max(deepest, level.capacity_bytes);
    }
    if (curve.levels.empty()) {
        append_row(table, "Prefetch probe", cpu, "-", "-", "-",
            "not measured: the latency curve resolved no cache level");
        return;
    }
    const PrefetchProbeResult result =
        measure_prefetch_probe(line_bytes, l1, l2, deepest);
    if (result.worksets[0].bytes == 0 && result.worksets[1].bytes == 0) {
        append_row(table, "Prefetch probe", cpu, "-", "-", "-",
            "not measured: no working set could be mapped");
        return;
    }
    print_prefetch_probe_tables(result, fallback_clock_ghz);

    const std::string worksets = worksets_text(result);
    append_row(table, "Hardware prefetch stride reach", cpu,
        "dependent loads, strides of 1 to 64 lines (1,2,3,4,6,8,...)", "-",
        worksets,
        describe_stride_reach(result, fallback_clock_ghz));
    append_row(table, "Stream restart cost", cpu,
        "sequential segments of 4 to 1024 lines", "-",
        result.worksets[1].bytes ? format_cache_capacity(result.worksets[1].bytes) : "-",
        describe_restart_cost(result, fallback_clock_ghz));
    for (int target = 0; target < 2; ++target) {
        std::string measurement;
        std::string bandwidth = "-";
        for (size_t c = 0; c < kPrefetchColumns; ++c) {
            if (kPrefetchColumnSpec[c].l2_target != (target == 1)) continue;
            const std::string part =
                describe_distance_column(result, c, fallback_clock_ghz);
            if (part.empty()) continue;
            if (!measurement.empty()) measurement += "; ";
            measurement += part;
            // The memory column's best rate as a bandwidth: one random stream.
            if (kPrefetchColumnSpec[c].workset == PREFETCH_MEMORY_WORKSET) {
                const DistanceSummary summary = summarize_distances(result, c);
                if (summary.best_ns > 0.0) {
                    char text[48];
                    std::snprintf(text, sizeof(text), "%.3f GB/s",
                        (line_bytes > 0 ? line_bytes : 64) / summary.best_ns);
                    bandwidth = std::string(text) + " (one random stream)";
                }
            }
        }
        append_row(table,
            target == 0 ? "Software prefetch distance (L1 target)"
                        : "Software prefetch distance (L2 target)",
            cpu,
            std::string(prefetch_instruction_name(target == 1)) +
                " ahead of a dependent chain",
            bandwidth, worksets,
            measurement.empty() ? "not measured" : measurement);
    }
}

} // namespace cpufb
