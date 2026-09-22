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

// Points of one plateau stay within this max/min band.
const double kPlateauBand = 1.10;
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
            if (next_low <= 0.0 || next_high / next_low > kPlateauBand) break;
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
// repeats only every `slots` laps.  Small working sets, where such
// prefetchers have the easiest job, get the most slots (16 per 64-byte line
// with 32-bit links); large ones need none.
//
// The price is the top of each step.  With one permutation every line is
// re-referenced exactly line_count accesses later, so a working set one line
// over the capacity misses on every access.  Across a change of permutation
// the reuse distance varies between 1 and 2 line_count, and a working set of
// n lines in a cache of c lines keeps a share of its hits that falls off
// roughly as (c/n)^2.5 (0.55 at 1.25x, 0.17 at 2x, 0.04 at 4x the capacity
// on an x86 core): the latency still jumps at the capacity, but closes the
// last tenth of the step only near 2.5x.  So the width of a rise is not
// evidence against a boundary; where the capacity sits within it is (see
// CacheLevelEstimate::gradual).
size_t slots_for(size_t line_count, size_t stride_words)
{
    const size_t kTargetPairs = 65536;
    const size_t wanted = (kTargetPairs + line_count - 1) / line_count;
    return std::max<size_t>(1, std::min(wanted, stride_words));
}

double measure_pointer_chase(CacheChaseKernel chase, int32_t *buffer,
    uint64_t working_set_bytes, size_t line_size, size_t group_lines,
    uint64_t seed)
{
    const size_t stride = std::max(sizeof(int32_t), line_size);
    const size_t line_count = std::max<size_t>(2, working_set_bytes / stride);
    const size_t stride_words = stride / sizeof(int32_t);
    const size_t slots = slots_for(line_count, stride_words);
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
    const size_t lap = line_count * slots;

    const size_t int_max = static_cast<size_t>(std::numeric_limits<int>::max());
    // Two full laps place every line at its steady-state cache level.
    chase(static_cast<int>(std::min(int_max, std::max<size_t>(10000, lap * 2))),
        buffer);

    const int probe_iterations = static_cast<int>(
        std::min<size_t>(std::max<size_t>(50000, lap), 2000000));
    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    chase(probe_iterations, buffer);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    const double probe_ns = elapsed_ns(start, end) / probe_iterations;
    if (!(probe_ns > 0.0)) return 0.0;

    // About 5 ms per sample, but never less than one full lap.
    size_t iterations = static_cast<size_t>(5e6 / probe_ns);
    iterations = std::max<size_t>(iterations, 50000);
    iterations = std::max(iterations, lap);
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
uint64_t choose_region_bytes(uint64_t max_bytes)
{
#if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    const long pages = sysconf(_SC_AVPHYS_PAGES);
    const long page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0 &&
        static_cast<uint64_t>(pages) * page / 4 >= kMemoryReferenceBytes)
        return std::max(max_bytes, kMemoryReferenceBytes);
    return max_bytes;
#else
    // No cheap way to ask (macOS): 1 GiB is small next to any Apple Silicon
    // configuration.
    return std::max(max_bytes, kMemoryReferenceBytes);
#endif
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
    if (!level.gradual) return "";
    char text[192];
    std::snprintf(text, sizeof(text),
        "probe: %s latency rises gradually from %s to %s (%.1fx) instead of "
        "stepping, so no capacity is read off it; prefetcher suspected",
        level.level.c_str(),
        format_approximate_capacity(level.rise_begin_bytes).c_str(),
        format_approximate_capacity(level.rise_end_bytes).c_str(),
        level.transition_width);
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
            "  %s: capacity %s, %.3f ns, jump %.2fx, rise %.2fx (%s to %s)%s\n",
            level.level.c_str(),
            format_approximate_capacity(level.capacity_bytes).c_str(),
            level.latency_ns, level.jump_ratio, level.transition_width,
            format_approximate_capacity(level.rise_begin_bytes).c_str(),
            format_approximate_capacity(level.rise_end_bytes).c_str(),
            level.gradual ? " gradual" : "");
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
        // enough above the plateau to ignore its noise.  It does not depend
        // on where the upper plateau is judged to begin, which is unreliable
        // when that plateau is noisy DRAM latency.
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
        level.capacity_bytes = points[capacity_index].working_set_bytes;
        level.latency_ns = below.latency_ns;
        level.jump_ratio = above.latency_ns / below.latency_ns;
        level.transition_width = transition_width(
            points, below, above, level.rise_begin_bytes, level.rise_end_bytes);
        // A capacity boundary makes the latency jump, so the threshold is
        // crossed where the rise starts; a slope crosses it partway up, and
        // then the crossing point says nothing about the capacity.
        level.gradual = level.rise_begin_bytes > 0 &&
            static_cast<double>(level.capacity_bytes) > kGradualRisePosition *
                    static_cast<double>(level.rise_begin_bytes);
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
    const bool group_by_page = !huge_pages && page_bytes < kLargePageBytes;
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
    for (size_t i = 0; i < sizes.size(); ++i) {
        CacheLatencyPoint point;
        point.working_set_bytes = sizes[i];
        point.latency_ns = measure_pointer_chase(chase, buffer, sizes[i], line,
            group_lines, 0x4350554642ULL + i * 0x9e3779b97f4a7c15ULL);
        result.points.push_back(point);
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
