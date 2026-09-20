#include "cache_curve.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using cpufb::build_cache_curve_sizes;
using cpufb::CacheLatencyPoint;
using cpufb::CacheLevelEstimate;
using cpufb::estimate_cache_levels;

namespace {

constexpr uint64_t kKiB = 1024;

struct ExpectedLevel
{
    const char *level;
    uint64_t capacity_kib;
};

// Latency of the first grid point whose working set exceeds each boundary.
struct Step
{
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
    const std::vector<CacheLevelEstimate> levels =
        estimate_cache_levels(points);
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

    ok &= expect_levels("no L3", make_curve(1.0, {{128, 5.0}, {16384, 95.0}}),
        {{"L1", 128}, {"L2", 16384}});

    // A contended L1 softens the knee below 64 KiB.  Points that still
    // perform like the plateau count as fitting; the ramp does not.
    std::vector<CacheLatencyPoint> soft =
        make_curve(1.5, {{64, 4.6}, {1024, 22.0}});
    for (CacheLatencyPoint &point : soft) {
        if (point.working_set_bytes == 56 * kKiB) point.latency_ns = 1.7;
        if (point.working_set_bytes == 64 * kKiB) point.latency_ns = 2.0;
    }
    ok &= expect_levels("soft knee", soft, {{"L1", 64}, {"L2", 1024}});

    // Cluster-shared L2 that softens above its capacity, after an Apple M4
    // Pro (128 KiB L1, 16 MiB L2): 20 MiB reads 24.5 ns between the 7.9 ns
    // plateau and ~110 ns DRAM.  The geometric midpoint placed L2 at 20 MiB.
    std::vector<CacheLatencyPoint> shared_l2 =
        make_curve(0.9, {{128, 7.9}, {16384, 113.0}});
    for (CacheLatencyPoint &point : shared_l2) {
        const uint64_t mib = point.working_set_bytes / (1024 * kKiB);
        if (mib == 20) point.latency_ns = 24.5;
        if (mib == 24) point.latency_ns = 38.2;
        if (mib == 28) point.latency_ns = 45.0;
        if (mib == 32) point.latency_ns = 50.6;
        if (mib == 40) point.latency_ns = 80.0;
        if (mib == 48) point.latency_ns = 105.8;
    }
    ok &= expect_levels("shared L2", shared_l2, {{"L1", 128}, {"L2", 16384}});

    // Translation cost growing slowly across the L2 plateau (4 KiB pages) is
    // drift within one level, not an extra level between L1 and L2.
    ok &= expect_levels("translation drift",
        make_curve(1.6, {{32, 4.7}, {1024, 24.0}}, 0.12, 128),
        {{"L1", 32}, {"L2", 1024}});

    // Measured on an isolated Kunpeng 920F core (32 KiB L1, 768 KiB L2, no
    // L3, transparent huge pages).  The DRAM plateau is noisy, so a rule that
    // depends on where it starts reads 1280 KiB here.
    static const struct
    {
        uint64_t kib;
        double latency_ns;
    } kKunpeng920F[] = {
        {4, 3.594},
        {5, 3.490},
        {6, 3.464},
        {7, 3.445},
        {8, 3.366},
        {10, 3.516},
        {12, 3.876},
        {14, 3.854},
        {16, 3.773},
        {20, 3.646},
        {24, 3.485},
        {28, 3.417},
        {32, 3.334},
        {40, 9.997},
        {48, 10.001},
        {56, 10.003},
        {64, 10.005},
        {80, 10.000},
        {96, 10.002},
        {112, 10.003},
        {128, 10.001},
        {160, 9.997},
        {192, 10.001},
        {224, 10.001},
        {256, 9.998},
        {320, 10.000},
        {384, 10.000},
        {448, 10.001},
        {512, 10.001},
        {640, 10.001},
        {768, 10.017},
        {896, 48.307},
        {1024, 97.190},
        {1280, 85.911},
        {1536, 105.296},
        {1792, 113.811},
        {2048, 109.800},
        {2560, 96.627},
        {3072, 78.131},
        {3584, 86.129},
        {4096, 86.443},
        {5120, 88.264},
        {6144, 89.525},
        {7168, 93.516},
        {8192, 97.203},
        {10240, 109.591},
        {12288, 116.506},
        {14336, 120.739},
        {16384, 123.747},
        {20480, 128.371},
        {24576, 131.758},
        {28672, 133.217},
        {32768, 134.513},
        {40960, 135.800},
        {49152, 136.760},
        {57344, 138.011},
        {65536, 138.960},
    };
    std::vector<CacheLatencyPoint> kunpeng;
    for (const auto &sample : kKunpeng920F) {
        CacheLatencyPoint point;
        point.working_set_bytes = sample.kib * kKiB;
        point.latency_ns = sample.latency_ns;
        kunpeng.push_back(point);
    }
    ok &= expect_levels("Kunpeng 920F", kunpeng, {{"L1", 32}, {"L2", 768}});

    ok &= expect_levels("flat curve", make_curve(2.0, {}), {});

    const std::vector<uint64_t> sizes =
        build_cache_curve_sizes(8 * 1024 * kKiB);
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
