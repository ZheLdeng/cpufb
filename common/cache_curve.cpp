#include "cache_curve.hpp"

#include <algorithm>
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
// Adjacent plateaus closer than this are one level drifting (for example
// under growing translation cost), not a new cache level.
const double kMinimumLevelRatio = 1.25;
const double kMinimumLevelDeltaNs = 0.20;

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

struct Plateau {
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
std::vector<size_t> build_ring_order(size_t line_count, size_t group_lines,
    uint64_t seed)
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
    std::rotate(order.begin(), std::find(order.begin(), order.end(), 0),
        order.end());
    return order;
}

double measure_pointer_chase(CacheChaseKernel chase, int64_t *buffer,
    uint64_t working_set_bytes, size_t line_size, size_t group_lines,
    uint64_t seed)
{
    const size_t stride = std::max(sizeof(int64_t), line_size);
    const size_t line_count = std::max<size_t>(2, working_set_bytes / stride);
    const size_t stride_words = stride / sizeof(int64_t);
    const std::vector<size_t> order =
        build_ring_order(line_count, group_lines, seed);
    for (size_t i = 0; i < line_count; ++i)
        buffer[order[i] * stride_words] = static_cast<int64_t>(
            order[(i + 1) % line_count] * stride_words);

    const size_t int_max = static_cast<size_t>(std::numeric_limits<int>::max());
    // Two full laps place every line at its steady-state cache level.
    chase(static_cast<int>(std::min(int_max,
        std::max<size_t>(10000, line_count * 2))), buffer);

    const int probe_iterations = static_cast<int>(std::min<size_t>(
        std::max<size_t>(50000, line_count), 2000000));
    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    chase(probe_iterations, buffer);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    const double probe_ns = elapsed_ns(start, end) / probe_iterations;
    if (!(probe_ns > 0.0)) return 0.0;

    // About 5 ms per sample, but never less than one full lap.
    size_t iterations = static_cast<size_t>(5e6 / probe_ns);
    iterations = std::max<size_t>(iterations, 50000);
    iterations = std::max(iterations, line_count);
    iterations = std::min(iterations, int_max);
    std::vector<double> samples;
    for (int sample = 0; sample < 5; ++sample) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        chase(static_cast<int>(iterations), buffer);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        samples.push_back(elapsed_ns(start, end) / iterations);
    }
    return median_of(samples);
}

} // namespace

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

std::vector<CacheLevelEstimate> estimate_cache_levels(
    const std::vector<CacheLatencyPoint> &points)
{
    std::vector<CacheLevelEstimate> levels;
    const std::vector<Plateau> plateaus = find_plateaus(points);
    for (size_t i = 0; i + 1 < plateaus.size(); ++i) {
        const Plateau &below = plateaus[i];
        const Plateau &above = plateaus[i + 1];
        if (above.latency_ns < below.latency_ns * kMinimumLevelRatio ||
            above.latency_ns - below.latency_ns < kMinimumLevelDeltaNs)
            continue;

        // A cyclic ring re-references every line after exactly one lap, so
        // the ideal curve is a step: all hits up to the capacity, all misses
        // from the first larger working set, where the next plateau starts.
        // Real curves soften only below the step, because a ring that fills
        // the level completely competes with every other resident line
        // (stack, code, an SMT sibling).  The capacity is therefore the last
        // working set before the upper plateau begins, not the start or the
        // midpoint of the ramp, which would under-report by a grid step.
        size_t capacity_index = above.first > below.last
            ? above.first - 1 : below.last;
        CacheLevelEstimate level;
        level.level = "L" + std::to_string(levels.size() + 1);
        level.capacity_bytes = points[capacity_index].working_set_bytes;
        level.latency_ns = below.latency_ns;
        level.jump_ratio = above.latency_ns / below.latency_ns;
        levels.push_back(level);
    }
    return levels;
}

CacheCurveResult measure_cache_curve(CacheChaseKernel chase, int line_size,
    uint64_t max_bytes)
{
    CacheCurveResult result;
    if (chase == nullptr || max_bytes < 8 * 1024) return result;
    const size_t line = line_size > 0 ? static_cast<size_t>(line_size) : 64;

    const bool huge_pages = transparent_huge_pages_available();
    const long page_size = sysconf(_SC_PAGESIZE);
    const size_t page_bytes = page_size > 0 ? static_cast<size_t>(page_size)
        : 4096;
    const size_t group_lines = huge_pages ? 0
        : std::max<size_t>(1, page_bytes / line);
    result.translation_mode = huge_pages ? "huge pages" : "page-grouped order";

    void *allocation = nullptr;
#ifdef __linux__
    const size_t huge_page_size = 2ULL * 1024 * 1024;
    const size_t mapping_bytes = static_cast<size_t>(max_bytes) + huge_page_size;
    void *mapping = mmap(nullptr, mapping_bytes, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return result;
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(mapping) +
        huge_page_size - 1) & ~(huge_page_size - 1);
    allocation = reinterpret_cast<void *>(aligned);
    if (huge_pages)
        (void)madvise(allocation, static_cast<size_t>(max_bytes), MADV_HUGEPAGE);
#else
    if (posix_memalign(&allocation, std::max(page_bytes, line),
            static_cast<size_t>(max_bytes)) != 0)
        return result;
#endif
    std::memset(allocation, 0, static_cast<size_t>(max_bytes));
    int64_t *buffer = static_cast<int64_t *>(allocation);

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
    result.levels = estimate_cache_levels(result.points);
    return result;
}

} // namespace cpufb
