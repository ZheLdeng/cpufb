#include "cache_curve.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>

#include <unistd.h>
#ifdef __linux__
#include <sys/mman.h>
#endif

namespace cpufb {

namespace {

// A plateau ends where the latency starts climbing, and how fast it climbs
// is measured between neighbouring samples: log latency over log working
// set, so a doubling of latency across a doubling of the working set is 1.0.
// Within a level this stays near zero (0.05 to 0.2 on every curve measured),
// and a boundary is 1.4 to 10.
//
// The rule this replaces asked that the whole run stay within a 1.10 band of
// its own minimum, which made where a plateau ends depend on where it began
// and on how far a slow drift had accumulated.  Two platforms landed on the
// edge of that band at once: a MediaTek MT6993 core had a run measuring
// 1.099 against the 1.10 limit, so a 0.003 ns wobble decided whether it
// found two cache levels or three, and an Apple M4 Pro's L2, which climbs
// across its own range, was cut in a different place run to run, which moved
// its capacity between 8 and 20 MiB.
const double kPlateauSlope = 0.30;
// No real plateau drifts this far within itself; this only stops a curve
// that creeps everywhere from merging into one segment.
const double kMaximumPlateauSpan = 4.0;
const size_t kMinimumPlateauPoints = 3;
// Adjacent plateaus closer than this are one level drifting, not a new cache
// level.  Translation cost is the usual cause: on a Kunpeng 920F DRAM latency
// climbs 1.5x between 8 MiB and 64 MiB as the huge-page TLB reach is exceeded,
// while the smallest real step observed so far (L1 to L2) is 2.8x.
const double kMinimumLevelRatio = 1.75;
const double kMinimumLevelDeltaNs = 0.20;
// Random DRAM access is 3-10x slower than the slowest cache level, so half of
// the measured memory latency separates the two with margin on both sides.
const double kMemoryLatencyFraction = 0.5;
// The memory reference walks this many lines spread over a region far larger
// than any cache: 4M lines are 256 MiB of distinct cache lines.
const size_t kMemoryReferenceLines = 4u << 20;
const uint64_t kMemoryReferenceBytes = 1ULL << 30;

double elapsed_ns(const timespec &start, const timespec &end)
{
    return (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
}

double median_of(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle]
                             : (values[middle - 1] + values[middle]) / 2.0;
}

struct Plateau
{
    size_t first;
    size_t last;
    double latency_ns;
};

std::vector<Plateau> find_plateaus(const std::vector<CacheLatencyPoint> &points)
{
    std::vector<Plateau> plateaus;
    size_t start = 0;
    while (start < points.size()) {
        double low = points[start].latency_ns;
        double high = low;
        size_t end = start;
        while (end + 1 < points.size()) {
            const double next = points[end + 1].latency_ns;
            const double next_low = std::min(low, next);
            const double next_high = std::max(high, next);
            if (next_low <= 0.0 || next_high / next_low > kMaximumPlateauSpan)
                break;
            const double rise =
                std::fabs(std::log(next / points[end].latency_ns));
            const double step = std::log(
                static_cast<double>(points[end + 1].working_set_bytes) /
                static_cast<double>(points[end].working_set_bytes));
            if (step <= 0.0 || rise / step > kPlateauSlope) break;
            low = next_low;
            high = next_high;
            ++end;
        }
        if (low > 0.0 && end - start + 1 >= kMinimumPlateauPoints) {
            std::vector<double> values;
            for (size_t i = start; i <= end; ++i)
                values.push_back(points[i].latency_ns);
            plateaus.push_back({start, end, median_of(values)});
            start = end + 1;
        } else {
            ++start;
        }
    }
    return plateaus;
}

bool transparent_huge_pages_available()
{
#ifdef __linux__
    std::ifstream file("/sys/kernel/mm/transparent_hugepage/enabled");
    std::string policy;
    if (!file || !std::getline(file, policy)) return false;
    return policy.find("[never]") == std::string::npos;
#else
    return false;
#endif
}

// Ring order over `line_count` lines.  group_lines == 0 shuffles globally;
// otherwise groups are visited in random order and shuffled internally.  The
// ring always starts at line 0 because the chase kernels start at word 0.
std::vector<size_t> build_ring_order(
    size_t line_count, size_t group_lines, uint64_t seed)
{
    std::mt19937_64 random(seed);
    if (group_lines == 0 || group_lines >= line_count) group_lines = line_count;
    const size_t group_count = (line_count + group_lines - 1) / group_lines;
    std::vector<size_t> groups(group_count);
    std::iota(groups.begin(), groups.end(), 0);
    std::shuffle(groups.begin(), groups.end(), random);

    std::vector<size_t> order;
    order.reserve(line_count);
    for (size_t group : groups) {
        const size_t begin = group * group_lines;
        const size_t end = std::min(line_count, begin + group_lines);
        const size_t offset = order.size();
        for (size_t line = begin; line < end; ++line) order.push_back(line);
        std::shuffle(order.begin() + offset, order.end(), random);
    }
    std::rotate(
        order.begin(), std::find(order.begin(), order.end(), 0), order.end());
    return order;
}

// A ring is a fixed sequence of line addresses, and a core with a temporal
// (correlating) prefetcher learns it: once it has seen every "line A is
// followed by line B" pair it fetches B while A is being used, and misses
// disappear.  On an Arm big core (MediaTek MT6993 cpu7) that kept a 256 KiB
// ring at L1 latency and smeared the L1 and L2 steps into a gradual slope,
// while the little cores showed textbook steps.  So the ring is built from
// several distinct permutations of the same lines, one per link slot of the
// line, played one after the other: the cache sees the same set of lines,
// but the pair sequence is `slots` times longer than the working set and
// repeats only every `slots` laps.
//
// The count is the same at every working set, and that matters more than
// its value.  It used to fall as the working set grew, on the reasoning
// that a prefetcher has no chance of memorising millions of pairs anyway;
// but each drop is a change of method in the middle of a sweep, and it
// shows up as a step in the curve that no cache put there.  An MT6993 core
// answered 3.5 MiB in 6.7 ns and 4 MiB in 4.5 ns, faster with the larger
// set, exactly where the count fell from two to one.
//
// The price is the top of each step.  With one permutation every line is
// re-referenced exactly line_count accesses later, so a working set one line
// over the capacity misses on every access.  Across a change of permutation
// the reuse distance varies between 1 and 2 line_count, and a working set of
// n lines in a cache of c lines keeps a share of its hits that falls off
// roughly as (c/n)^2.5 (0.55 at 1.25x, 0.17 at 2x, 0.04 at 4x the capacity
// on an x86 core): the latency still jumps at the capacity, but closes the
// last tenth of the step only near 2.5x.  A wide rise is therefore not
// evidence against a boundary, and neither is where the capacity sits inside
// it: an Apple M4's cluster-shared L2, whose 16 MiB every other check
// confirms, sits at 1.3-1.6 of the start of its rise because the plateau
// below it drifts upward, while the MT6993 core whose plateau a prefetcher
// extends sits at 0.94.  Both were tried as tests and both withheld correct
// capacities; see describe_prefetch_doubt() for what the curve does say.
size_t slots_for(size_t stride_words)
{
    // One per link slot of a line: 16 in a 64-byte line, 32 in a 128-byte
    // one, capped so that building the ring for the largest working sets
    // stays a small part of the sweep.
    const size_t kMaximumSlots = 16;
    return std::max<size_t>(1, std::min(stride_words, kMaximumSlots));
}

double measure_pointer_chase(CacheChaseKernel chase, int32_t *buffer,
    uint64_t working_set_bytes, size_t line_size, size_t group_lines,
    uint64_t seed)
{
    const size_t stride = std::max(sizeof(int32_t), line_size);
    const size_t line_count = std::max<size_t>(2, working_set_bytes / stride);
    const size_t stride_words = stride / sizeof(int32_t);
    const size_t slots = slots_for(stride_words);
    // Node (slot k, line l) is word k of line l; the ring visits slot 0's
    // permutation, then slot 1's, ... and returns to slot 0.
    auto node = [&](size_t slot, size_t line) {
        return static_cast<int32_t>(line * stride_words + slot);
    };
    std::vector<size_t> order = build_ring_order(line_count, group_lines, seed);
    for (size_t slot = 0; slot < slots; ++slot) {
        const std::vector<size_t> next_order = slot + 1 < slots
            ? build_ring_order(
                  line_count, group_lines, seed + 0x9e37 * (slot + 1))
            : std::vector<size_t>();
        const std::vector<size_t> &following =
            slot + 1 < slots ? next_order : order;
        for (size_t i = 0; i + 1 < line_count; ++i)
            buffer[node(slot, order[i])] = node(slot, order[i + 1]);
        // Last line of this permutation to the first line of the next slot's.
        buffer[node(slot, order[line_count - 1])] =
            node((slot + 1) % slots, following[0]);
        if (slot + 1 < slots) order = next_order;
    }
    // How much work a sample is worth is set by the working set, not by the
    // number of permutations: a pass of line_count accesses already touches
    // every line once, whichever permutation it is in.  Tying it to the ring
    // length instead made a sample `slots` times longer, which at 128 MiB
    // was 32M dependent misses per sample.
    const size_t pass = line_count;

    const size_t int_max = static_cast<size_t>(std::numeric_limits<int>::max());
    // Two passes place every line at its steady-state cache level.
    chase(
        static_cast<int>(std::min(int_max, std::max<size_t>(10000, pass * 2))),
        buffer);

    const int probe_iterations = static_cast<int>(
        std::min<size_t>(std::max<size_t>(50000, pass), 2000000));
    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    chase(probe_iterations, buffer);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    const double probe_ns = elapsed_ns(start, end) / probe_iterations;
    if (!(probe_ns > 0.0)) return 0.0;

    // About 5 ms per sample, but never less than one full lap.
    size_t iterations = static_cast<size_t>(5e6 / probe_ns);
    iterations = std::max<size_t>(iterations, 50000);
    iterations = std::max(iterations, pass);
    iterations = std::min(iterations, int_max);
    // Everything that disturbs a sample (another core of a shared L2, an
    // interrupt, a migration on an unpinned macOS thread) adds latency and
    // nothing removes it, so the minimum is the estimate.  The median let
    // single points dip or bump by several ns on an Apple M4, which moved the
    // L2 estimate between 12 and 16 MiB from run to run.
    double best = 0.0;
    for (int sample = 0; sample < 7; ++sample) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        chase(static_cast<int>(iterations), buffer);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double latency = elapsed_ns(start, end) / iterations;
        if (latency > 0.0 && (best == 0.0 || latency < best)) best = latency;
    }
    return best;
}

// Dependent-load latency of main memory: a ring over lines scattered through
// `bytes` of memory, too many and too far apart for any cache to hold.  One
// cold lap is enough, since nothing here can become cache resident.
double measure_memory_latency(
    CacheChaseKernel chase, int32_t *buffer, uint64_t bytes, uint64_t seed)
{
    const size_t stride =
        std::max<size_t>(64, bytes / kMemoryReferenceLines) / 64 * 64;
    const size_t line_count = bytes / stride;
    const size_t stride_words = stride / sizeof(int32_t);
    const std::vector<size_t> order = build_ring_order(line_count, 0, seed);
    for (size_t i = 0; i < line_count; ++i)
        buffer[order[i] * stride_words] =
            static_cast<int32_t>(order[(i + 1) % line_count] * stride_words);

    const int steps = static_cast<int>(std::min<size_t>(
        line_count / 4, static_cast<size_t>(std::numeric_limits<int>::max())));
    double best = 0.0;
    for (int sample = 0; sample < 3; ++sample) {
        timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        chase(steps, buffer);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double latency = elapsed_ns(start, end) / steps;
        if (latency > 0.0 && (best == 0.0 || latency < best)) best = latency;
    }
    return best;
}

// Size of the mapping: the sweep itself, grown to the memory-reference region
// when a quarter of the available memory allows it.
// Memory this process could get without pushing the machine into swap.
// sysconf(_SC_AVPHYS_PAGES) answers with MemFree, which on any host that has
// been up for a while is a small fraction of it: a Kunpeng 920 with 14.4 GB
// available reported 0.90 GB, so the memory reference below was skipped and
// every capacity it measured carried a warning that the chase had been
// prefetched, next to rows that agreed with the OS exactly.
uint64_t available_memory_bytes()
{
#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    while (meminfo >> key) {
        unsigned long long value = 0;
        if (key == "MemAvailable:" && meminfo >> value) return value * 1024;
        std::getline(meminfo, key);
    }
#endif
#if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    const long pages = sysconf(_SC_AVPHYS_PAGES);
    const long page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0)
        return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page);
#endif
    return 0;
}

uint64_t choose_region_bytes(uint64_t max_bytes)
{
    const uint64_t available = available_memory_bytes();
    // No cheap way to ask on macOS, where this returns 0: 1 GiB is small next
    // to any Apple Silicon configuration.
    if (available == 0 || available / 4 >= kMemoryReferenceBytes)
        return std::max(max_bytes, kMemoryReferenceBytes);
    return max_bytes;
}

} // namespace

// Rise bounds are interpolated, so they are rounded for the note.
std::string format_approximate_capacity(uint64_t bytes)
{
    char text[32];
    if (bytes >= 1024 * 1024)
        std::snprintf(
            text, sizeof(text), "%.1f MiB", bytes / (1024.0 * 1024.0));
    else
        std::snprintf(text, sizeof(text), "%.0f KiB", bytes / 1024.0);
    return text;
}

std::string describe_transition(const CacheLevelEstimate &level)
{
    if (level.transition_width <= kWideRiseWidth || level.rise_begin_bytes == 0)
        return "";
    char text[160];
    std::snprintf(text, sizeof(text),
        "latency climbed over %s to %s (%.1fx), so the boundary is not sharp",
        format_approximate_capacity(level.rise_begin_bytes).c_str(),
        format_approximate_capacity(level.rise_end_bytes).c_str(),
        level.transition_width);
    return text;
}

// A pointer chase over a working set far larger than any cache has to miss,
// and a miss costs what memory costs.  A MediaTek MT6993 big core answered a
// 128 MiB chase in 12.6 ns, 25 cycles at its 2 GHz, which no DRAM does: its
// prefetcher followed the ring at every size.  Its L1 plateau then reached
// 128 KiB for a 64 KiB cache, and the curve carried no other sign of it.
std::string describe_prefetch_doubt(const CacheCurveResult &result)
{
    if (result.reached_memory || result.points.empty()) return "";
    const double deepest = result.points.back().latency_ns;
    // 40 cycles at 4 GHz, below any DRAM and above any cache hit.
    const double kMemoryFloorNs = 10.0;
    if (deepest >= kMemoryFloorNs && result.memory_latency_ns > 0.0 &&
        deepest >= kMemoryLatencyFraction * result.memory_latency_ns)
        return "";
    char text[192];
    std::snprintf(text, sizeof(text),
        "a %s working set still answered in %.1f ns, so the chase was "
        "prefetched throughout and every capacity here may be too large",
        format_approximate_capacity(result.points.back().working_set_bytes)
            .c_str(),
        deepest);
    return text;
}

void debug_print_cache_curve(const CacheCurveResult &result)
{
    if (std::getenv("CPUFB_DEBUG_CACHE_CURVE") == nullptr) return;
    std::fprintf(stderr, "cache curve (%s), memory reference %.1f ns:\n",
        result.translation_mode.c_str(), result.memory_latency_ns);
    for (const CacheLatencyPoint &point : result.points)
        std::fprintf(stderr, "  %8llu KB %8.3f ns/load\n",
            static_cast<unsigned long long>(point.working_set_bytes / 1024),
            point.latency_ns);
    for (const CacheLevelEstimate &level : result.levels)
        std::fprintf(stderr,
            "  %s: capacity %s, %.3f ns, jump %.2fx, plateau drift %.2fx, "
            "rise %.2fx (%s to %s)%s\n",
            level.level.c_str(),
            format_approximate_capacity(level.capacity_bytes).c_str(),
            level.latency_ns, level.jump_ratio, level.plateau_drift,
            level.transition_width,
            format_approximate_capacity(level.rise_begin_bytes).c_str(),
            format_approximate_capacity(level.rise_end_bytes).c_str(),
            level.transition_width > kWideRiseWidth ? " wide" : "");
    const std::string doubt = describe_prefetch_doubt(result);
    if (!doubt.empty()) std::fprintf(stderr, "  %s\n", doubt.c_str());
}

std::vector<uint64_t> build_cache_curve_sizes(uint64_t max_bytes)
{
    std::vector<uint64_t> sizes;
    for (uint64_t base = 4 * 1024; base <= max_bytes; base *= 2) {
        for (uint64_t quarter = 4; quarter < 8; ++quarter) {
            const uint64_t size = base / 4 * quarter;
            if (size <= max_bytes) sizes.push_back(size);
        }
    }
    return sizes;
}

// Working set at which the latency first reaches `target`, log-interpolated
// between the grid point below and the one at or above it.
uint64_t crossing_bytes(const std::vector<CacheLatencyPoint> &points,
    size_t first, size_t last, double target)
{
    for (size_t j = first; j <= last; ++j) {
        if (points[j].latency_ns < target) continue;
        if (j == first || points[j - 1].latency_ns >= target ||
            points[j - 1].latency_ns <= 0.0)
            return points[j].working_set_bytes;
        const double fraction = std::log(target / points[j - 1].latency_ns) /
            std::log(points[j].latency_ns / points[j - 1].latency_ns);
        const double lower =
            static_cast<double>(points[j - 1].working_set_bytes);
        const double upper = static_cast<double>(points[j].working_set_bytes);
        return static_cast<uint64_t>(lower * std::pow(upper / lower, fraction));
    }
    return 0;
}

// Working-set ratio over which the latency climbs from 10% to 90% of the
// step between two plateaus, on a log scale, with the two crossings.
double transition_width(const std::vector<CacheLatencyPoint> &points,
    const Plateau &below, const Plateau &above, uint64_t &begin, uint64_t &end)
{
    const double log_low = std::log(below.latency_ns);
    const double log_high = std::log(above.latency_ns);
    begin = crossing_bytes(points, below.first, above.last,
        std::exp(log_low + 0.1 * (log_high - log_low)));
    end = crossing_bytes(points, below.first, above.last,
        std::exp(log_low + 0.9 * (log_high - log_low)));
    if (begin == 0 || end == 0) return 1.0;
    return static_cast<double>(end) / begin;
}

std::vector<CacheLevelEstimate> estimate_cache_levels(
    const std::vector<CacheLatencyPoint> &points, double memory_latency_ns,
    bool *reached_memory)
{
    std::vector<CacheLevelEstimate> levels;
    if (reached_memory != nullptr) *reached_memory = false;
    const std::vector<Plateau> plateaus = find_plateaus(points);
    // A plateau is main memory, not a cache level, when it is as slow as a
    // working set that cannot fit in any cache.  Without that reference every
    // plateau but the last is taken to be a cache.
    auto is_memory = [&](const Plateau &plateau) {
        return memory_latency_ns > 0.0 &&
            plateau.latency_ns >= kMemoryLatencyFraction * memory_latency_ns;
    };
    for (size_t i = 0; i + 1 < plateaus.size(); ++i) {
        const Plateau &below = plateaus[i];
        const Plateau &above = plateaus[i + 1];
        // Steps between two memory plateaus are translation or NUMA effects.
        if (is_memory(below)) break;
        if (above.latency_ns < below.latency_ns * kMinimumLevelRatio ||
            above.latency_ns - below.latency_ns < kMinimumLevelDeltaNs)
            continue;
        if (is_memory(above) && reached_memory != nullptr)
            *reached_memory = true;

        // A cyclic ring re-references every line after exactly one lap, so
        // the ideal curve is a step at the capacity.  Real curves soften on
        // either side of it: below when the level is shared with other
        // activity, above when replacement is not strict LRU or the level is
        // shared by a cluster (a 16 MiB Apple M4 L2 still reads 24 ns at
        // 20 MiB, between its 8 ns plateau and 110 ns DRAM).  A working set
        // fits a level while it still performs like that level, so the
        // boundary is one third of the way up the step on a log scale: below
        // the geometric midpoint, which accepted that 20 MiB point, yet far
        // enough above the plateau to ignore its noise.
        //
        // Choosing the last sample below that threshold makes the answer a
        // grid point, which is exact where the step is sharp and quantized
        // where it is not: an Apple M4 Pro reported 8 to 20 MiB for its
        // 16 MiB L2 over twenty runs, because the threshold fell within
        // 0.04 ns of a sample.  Taking the steepest interval instead was
        // tried, and on a machine whose L3 never forms a plateau it returned
        // that L3's boundary as the L2 capacity.  The threshold stays until
        // there are curves to choose a better rule from; what improved this
        // round is the curve, not the rule reading it.
        const double threshold =
            std::cbrt(below.latency_ns * below.latency_ns * above.latency_ns);
        size_t capacity_index = below.last;
        for (size_t j = below.last; j < above.first; ++j) {
            if (points[j].latency_ns < threshold)
                capacity_index = j;
            else
                break;
        }
        CacheLevelEstimate level;
        level.level = "L" + std::to_string(levels.size() + 1);
        // How flat the plateau this level sits on actually is.  A level whose
        // own latency climbs before its boundary pushes the 10% crossing of
        // the step down below the capacity, which is why an Apple M4 Pro's
        // L2 reads far above the start of its rise; no other output says so.
        level.plateau_drift = points[below.first].latency_ns > 0.0
            ? points[below.last].latency_ns / points[below.first].latency_ns
            : 1.0;
        level.capacity_bytes = points[capacity_index].working_set_bytes;
        level.latency_ns = below.latency_ns;
        level.jump_ratio = above.latency_ns / below.latency_ns;
        level.transition_width = transition_width(
            points, below, above, level.rise_begin_bytes, level.rise_end_bytes);
        levels.push_back(level);
    }
    return levels;
}

CacheCurveResult measure_cache_curve(
    CacheChaseKernel chase, int line_size, uint64_t max_bytes)
{
    CacheCurveResult result;
    if (chase == nullptr || max_bytes < 8 * 1024) return result;
    const size_t line = line_size > 0 ? static_cast<size_t>(line_size) : 64;

    const bool huge_pages = transparent_huge_pages_available();
    const long page_size = sysconf(_SC_PAGESIZE);
    const size_t page_bytes =
        page_size > 0 ? static_cast<size_t>(page_size) : 4096;
    // Page-grouped order keeps translation misses amortized when only 4 KiB
    // pages are available.  It is not used with 16 KiB or larger pages: the
    // TLB reach is then several MiB, and visiting a whole page at once lets a
    // region prefetcher serve most accesses (an Apple M4 read 14 ns at 48 MiB
    // instead of ~110 ns, non-monotonically, and L2 could not be placed).
    const size_t kLargePageBytes = 16 * 1024;
    // CPUFB_CHASE_NO_PAGE_GROUPS=1 shuffles globally instead, which costs a
    // translation miss per access and is the control for the other half of
    // that trade: visiting a whole page at once is exactly what lets a region
    // prefetcher serve it.  A MediaTek MT6993 answers a 128 MiB chase in
    // 26 ns under page-grouped order, which is not memory, and its curve
    // falls by 0.9 ns at 12 MiB; the same sweep on huge pages rises there.
    const char *no_groups = std::getenv("CPUFB_CHASE_NO_PAGE_GROUPS");
    const bool group_by_page = !huge_pages && page_bytes < kLargePageBytes &&
        (no_groups == nullptr || no_groups[0] != '1');
    const size_t group_lines =
        group_by_page ? std::max<size_t>(1, page_bytes / line) : 0;
    if (huge_pages)
        result.translation_mode = "huge pages";
    else if (group_by_page)
        result.translation_mode = "page-grouped order";
    else
        result.translation_mode = "large base pages";

    const uint64_t region_bytes = choose_region_bytes(max_bytes);
    void *allocation = nullptr;
#ifdef __linux__
    const size_t huge_page_size = 2ULL * 1024 * 1024;
    const size_t mapping_bytes =
        static_cast<size_t>(region_bytes) + huge_page_size;
    void *mapping = mmap(nullptr, mapping_bytes, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return result;
    const uintptr_t aligned =
        (reinterpret_cast<uintptr_t>(mapping) + huge_page_size - 1) &
        ~(huge_page_size - 1);
    allocation = reinterpret_cast<void *>(aligned);
    if (huge_pages)
        (void)madvise(
            allocation, static_cast<size_t>(region_bytes), MADV_HUGEPAGE);
#else
    if (posix_memalign(&allocation, std::max(page_bytes, line),
            static_cast<size_t>(region_bytes)) != 0)
        return result;
#endif
    std::memset(allocation, 0, static_cast<size_t>(region_bytes));
    int32_t *buffer = static_cast<int32_t *>(allocation);

    // Measured first, while nothing of the region is cache resident.
    if (region_bytes > max_bytes * 2)
        result.memory_latency_ns = measure_memory_latency(
            chase, buffer, region_bytes, 0x4d454d4f52595245ULL);

    const std::vector<uint64_t> sizes = build_cache_curve_sizes(max_bytes);
    auto sweep = [&](size_t lines_per_group) {
        std::vector<CacheLatencyPoint> points;
        for (size_t i = 0; i < sizes.size(); ++i) {
            CacheLatencyPoint point;
            point.working_set_bytes = sizes[i];
            point.latency_ns = measure_pointer_chase(chase, buffer, sizes[i],
                line, lines_per_group,
                0x4350554642ULL + i * 0x9e3779b97f4a7c15ULL);
            points.push_back(point);
        }
        return points;
    };
    result.points = sweep(group_lines);

    // Page-grouped order visits all the lines of a page in a row, which is
    // also the pattern a region prefetcher serves best, and on a MediaTek
    // MT6993 it served all of it: the deepest working set, 128 MiB, answered
    // in 26.6 ns against a 212 ns memory reference, the curve fell where it
    // should have risen, and the capacities read off it were a prefetcher's.
    // The same sweep shuffled globally reached 172 ns, rose throughout, and
    // agreed about L1.  So the grouping is kept for the translation misses it
    // saves, and dropped when the reference says it bought a curve that never
    // left the prefetcher.
    if (group_by_page && result.memory_latency_ns > 0.0 &&
        !result.points.empty() &&
        result.points.back().latency_ns <
            kMemoryLatencyFraction * result.memory_latency_ns) {
        result.points = sweep(0);
        result.translation_mode = "global order (page-grouped order never "
                                  "reached memory)";
    }
#ifdef __linux__
    munmap(mapping, mapping_bytes);
#else
    std::free(allocation);
#endif
    result.levels = estimate_cache_levels(
        result.points, result.memory_latency_ns, &result.reached_memory);
    return result;
}

} // namespace cpufb
