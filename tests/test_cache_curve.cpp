#include "cache_curve.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kKiB = 1024;

struct ExpectedLevel {
    const char *level;
    uint64_t capacity_kib;
};

// Latency of the first grid point whose working set exceeds each boundary.
struct Step {
    uint64_t capacity_kib;
    double latency_ns;
};

std::vector<CacheLatencyPoint> make_curve(double base_latency_ns,
    const std::vector<Step> &steps, double drift_per_point = 0.0,
    uint64_t drift_from_kib = 0)
{
    std::vector<CacheLatencyPoint> points;
    double drift = 0.0;
    for (uint64_t size : build_cache_curve_sizes(64 * 1024 * kKiB)) {
        double latency = base_latency_ns;
        for (const Step &step : steps)
            if (size > step.capacity_kib * kKiB) latency = step.latency_ns;
        if (drift_per_point > 0.0 && size > drift_from_kib * kKiB)
            drift += drift_per_point;
        CacheLatencyPoint point;
        point.working_set_bytes = size;
        point.latency_ns = latency + drift;
        points.push_back(point);
    }
    return points;
}

bool expect_levels(const char *name,
    const std::vector<CacheLatencyPoint> &points,
    const std::vector<ExpectedLevel> &expected)
{
    const std::vector<CacheLevelEstimate> levels = estimate_cache_levels(points);
    bool ok = levels.size() == expected.size();
    for (size_t i = 0; ok && i < expected.size(); ++i)
        ok = levels[i].level == expected[i].level &&
            levels[i].capacity_bytes == expected[i].capacity_kib * kKiB;
    if (ok) return true;

    std::cerr << name << ": got";
    for (const CacheLevelEstimate &level : levels)
        std::cerr << ' ' << level.level << '=' << level.capacity_bytes / kKiB
                  << "KiB";
    std::cerr << ", expected";
    for (const ExpectedLevel &level : expected)
        std::cerr << ' ' << level.level << '=' << level.capacity_kib << "KiB";
    std::cerr << '\n';
    return false;
}

} // namespace

int main()
{
    bool ok = true;

    // Sizes are recovered from the curve alone, including capacities that are
    // not powers of two (48 KiB L1, 1.25 MiB L2).
    ok &= expect_levels("ideal steps",
        make_curve(1.3, {{48, 4.5}, {1280, 20.0}, {16384, 90.0}}),
        {{"L1", 48}, {"L2", 1280}, {"L3", 16384}});

    ok &= expect_levels("no L3",
        make_curve(1.0, {{128, 5.0}, {16384, 95.0}}),
        {{"L1", 128}, {"L2", 16384}});

    // A contended L1 softens the knee over two grid points below 64 KiB; the
    // geometric midpoint must not slide to the start of the ramp.
    std::vector<CacheLatencyPoint> soft =
        make_curve(1.5, {{64, 4.6}, {1024, 22.0}});
    for (CacheLatencyPoint &point : soft) {
        if (point.working_set_bytes == 56 * kKiB) point.latency_ns = 1.9;
        if (point.working_set_bytes == 64 * kKiB) point.latency_ns = 2.4;
    }
    ok &= expect_levels("soft knee", soft, {{"L1", 64}, {"L2", 1024}});

    // Translation cost growing slowly across the L2 plateau (4 KiB pages) is
    // drift within one level, not an extra level between L1 and L2.
    ok &= expect_levels("translation drift",
        make_curve(1.6, {{32, 4.7}, {1024, 24.0}}, 0.12, 128),
        {{"L1", 32}, {"L2", 1024}});

    ok &= expect_levels("flat curve", make_curve(2.0, {}), {});

    const std::vector<uint64_t> sizes = build_cache_curve_sizes(8 * 1024 * kKiB);
    if (sizes.empty() || sizes.front() != 4 * kKiB ||
        sizes.back() != 8 * 1024 * kKiB) {
        std::cerr << "size grid bounds are wrong\n";
        ok = false;
    }
    for (size_t i = 1; i < sizes.size(); ++i) {
        if (sizes[i] <= sizes[i - 1] || sizes[i] * 4 > sizes[i - 1] * 5) {
            std::cerr << "size grid is not strictly quarter-octave at index "
                      << i << '\n';
            ok = false;
            break;
        }
    }
    return ok ? 0 : 1;
}
