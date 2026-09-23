#include "cache_bandwidth.hpp"
#include "cache_curve.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using cpufb::build_cache_curve_sizes;
using cpufb::CacheCurveResult;
using cpufb::CacheLatencyPoint;
using cpufb::CacheLevelEstimate;
using cpufb::describe_prefetch_doubt;
using cpufb::describe_transition;
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

// A curve recorded on real hardware, as CPUFB_DEBUG_CACHE_CURVE prints it.
struct MeasuredPoint
{
    uint64_t kib;
    double latency_ns;
};

std::vector<CacheLatencyPoint> measured_curve(
    const MeasuredPoint *samples, size_t count)
{
    std::vector<CacheLatencyPoint> curve;
    for (size_t i = 0; i < count; ++i) {
        CacheLatencyPoint point;
        point.working_set_bytes = samples[i].kib * 1024;
        point.latency_ns = samples[i].latency_ns;
        curve.push_back(point);
    }
    return curve;
}

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

    // Translation cost growing slowly across the L2 plateau (4 KiB pages) is
    // drift within one level, not an extra level between L1 and L2.
    ok &= expect_levels("translation drift",
        make_curve(1.6, {{32, 4.7}, {1024, 24.0}}, 0.12, 128),
        {{"L1", 32}, {"L2", 1024}});

    // Measured on an isolated Kunpeng 920F core (32 KiB L1, 768 KiB L2, no
    // L3, transparent huge pages).  The DRAM plateau is noisy, so a rule that
    // depends on where it starts reads 1280 KiB here.
    static const MeasuredPoint kKunpeng920F[] = {
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
    std::vector<CacheLatencyPoint> kunpeng = measured_curve(
        kKunpeng920F, sizeof(kKunpeng920F) / sizeof(*kKunpeng920F));
    ok &= expect_levels("Kunpeng 920F", kunpeng, {{"L1", 32}, {"L2", 768}});

    // Telling an L3 from memory.  With the latency of a working set that no
    // cache can hold as reference, the 920F curve ends in memory right after
    // L2: there is no L3, and the hierarchy is known to be complete.
    bool reached_memory = false;
    std::vector<CacheLevelEstimate> levels =
        estimate_cache_levels(kunpeng, 135.0, &reached_memory);
    if (levels.size() != 2 || !reached_memory) {
        std::cerr << "Kunpeng 920F with memory reference: expected exactly "
                     "L1 and L2 followed by memory\n";
        ok = false;
    }

    // A real third level is slower than L2 but far faster than memory.
    levels = estimate_cache_levels(
        make_curve(1.3, {{48, 4.5}, {1280, 20.0}, {16384, 90.0}}), 95.0,
        &reached_memory);
    if (levels.size() != 3 || levels[2].level != "L3" ||
        levels[2].capacity_bytes != 16384 * kKiB || !reached_memory) {
        std::cerr << "L3 before memory was not recognised\n";
        ok = false;
    }

    // A last level larger than the sweep: the curve never gets near memory
    // latency, so the list must not be reported as complete.
    levels = estimate_cache_levels(
        make_curve(1.3, {{48, 4.5}, {1280, 20.0}}), 95.0, &reached_memory);
    if (levels.size() != 2 || reached_memory) {
        std::cerr
            << "a sweep that ends inside L3 must not claim completeness\n";
        ok = false;
    }

    // A clean cache under the multi-permutation ring: a working set of n
    // lines in a cache of c lines keeps about (c/n)^2.5 of its hits past the
    // capacity (see measure_pointer_chase), so the step trails off over
    // ~2.5x.  A wide rise that starts at the capacity is still a step.
    std::vector<CacheLatencyPoint> tailed;
    for (uint64_t size : build_cache_curve_sizes(64 * 1024 * kKiB)) {
        CacheLatencyPoint point;
        point.working_set_bytes = size;
        const double n = static_cast<double>(size) / kKiB;
        auto hits = [](double lines, double capacity) {
            return lines <= capacity ? 1.0 : std::pow(capacity / lines, 2.5);
        };
        if (n <= 1024)
            point.latency_ns = 1.6 + (4.5 - 1.6) * (1.0 - hits(n, 32));
        else
            point.latency_ns = 4.5 + (95.0 - 4.5) * (1.0 - hits(n, 1024));
        tailed.push_back(point);
    }
    ok &= expect_levels("ring tail", tailed, {{"L1", 32}, {"L2", 1024}});

    // Measured on an Apple M4 Pro (128 KiB L1, 16 MiB cluster-shared L2, no
    // L3).  The L2 plateau is not flat: it drifts from 6.0 ns at 1 MiB to
    // 17.4 ns at 14 MiB, so the 10% crossing of the step to memory happens
    // at 10 MiB, well below the 16 MiB boundary.  A rule that asked for the
    // capacity to sit at the foot of its rise withheld this level in 16 of
    // 17 runs, on the one machine whose L2 every other check confirms.
    static const MeasuredPoint kAppleM4Pro[] = {{4, 0.887}, {5, 0.887},
        {6, 0.887}, {7, 0.887}, {8, 0.974}, {10, 0.971}, {12, 0.971},
        {14, 0.949}, {16, 0.956}, {20, 0.985}, {24, 0.947}, {28, 0.980},
        {32, 0.933}, {40, 0.988}, {48, 0.944}, {56, 0.992}, {64, 0.944},
        {80, 0.942}, {96, 0.951}, {112, 0.948}, {128, 1.003}, {160, 3.250},
        {192, 4.309}, {224, 5.024}, {256, 5.665}, {320, 5.917}, {384, 5.843},
        {448, 6.347}, {512, 6.326}, {640, 6.188}, {768, 6.457}, {896, 5.911},
        {1024, 6.026}, {1280, 6.047}, {1536, 6.035}, {1792, 6.024},
        {2048, 6.029}, {2560, 6.100}, {3072, 6.439}, {3584, 6.881},
        {4096, 7.235}, {5120, 8.214}, {6144, 8.476}, {7168, 8.609},
        {8192, 8.884}, {10240, 10.819}, {12288, 13.858}, {14336, 17.436},
        {16384, 16.632}, {20480, 45.404}, {24576, 98.157}, {28672, 103.886},
        {32768, 106.125}, {40960, 108.428}, {49152, 110.029}, {57344, 110.053},
        {65536, 113.365}, {81920, 117.492}, {98304, 119.001}, {114688, 119.927},
        {131072, 118.649}};
    std::vector<CacheLatencyPoint> apple_m4 =
        measured_curve(kAppleM4Pro, sizeof(kAppleM4Pro) / sizeof(*kAppleM4Pro));
    ok &= expect_levels("Apple M4 Pro", apple_m4, {{"L1", 128}, {"L2", 16384}});

    // Measured on a MediaTek MT6993 big core (cpu7), whose real L1 is
    // 64 KiB.  Its prefetcher follows the ring at every working set: 2.01 ns
    // holds to 128 KiB, and 128 MiB still answers in 12.6 ns, 25 cycles at
    // 2 GHz, which no DRAM does.  The curve therefore reads a 128 KiB L1 and
    // there is nothing in its shape to say otherwise -- a width test called
    // it a slope, a position test called it a clean step, and both were
    // wrong somewhere else.  What the curve does say is that it never
    // reached memory, and that is what has to be reported.
    static const MeasuredPoint kMediaTekCpu7[] = {{4, 2.009}, {5, 2.011},
        {6, 2.007}, {7, 1.998}, {8, 2.009}, {10, 2.008}, {12, 2.009},
        {14, 2.007}, {16, 2.009}, {20, 2.009}, {24, 2.012}, {28, 2.011},
        {32, 2.009}, {40, 2.012}, {48, 2.010}, {56, 2.017}, {64, 2.011},
        {80, 2.012}, {96, 2.012}, {112, 2.020}, {128, 2.009}, {160, 2.743},
        {192, 3.533}, {224, 3.934}, {256, 4.206}, {320, 4.578}, {384, 4.836},
        {448, 5.009}, {512, 5.157}, {640, 5.367}, {768, 5.456}, {896, 5.675},
        {1024, 5.764}, {1280, 6.196}, {1536, 6.529}, {1792, 6.989},
        {2048, 7.011}, {2560, 7.511}, {3072, 7.823}, {3584, 7.950},
        {4096, 7.734}, {5120, 7.610}, {6144, 7.482}, {7168, 7.503},
        {8192, 7.488}, {10240, 7.609}, {12288, 7.794}, {14336, 7.956},
        {16384, 8.046}, {20480, 8.233}, {24576, 8.496}, {28672, 8.969},
        {32768, 8.936}, {40960, 9.827}, {49152, 10.643}, {57344, 10.932},
        {65536, 11.231}, {81920, 11.820}, {98304, 12.201}, {114688, 12.304},
        {131072, 12.560}};
    {
        CacheCurveResult prefetched;
        prefetched.points = measured_curve(
            kMediaTekCpu7, sizeof(kMediaTekCpu7) / sizeof(*kMediaTekCpu7));
        prefetched.levels =
            estimate_cache_levels(prefetched.points, 0.0, &reached_memory);
        prefetched.reached_memory = reached_memory;
        if (prefetched.reached_memory) {
            std::cerr << "a curve that tops out at 12.6 ns claimed to have "
                         "reached memory\n";
            ok = false;
        }
        if (describe_prefetch_doubt(prefetched).empty()) {
            std::cerr << "a curve prefetched throughout was not doubted\n";
            ok = false;
        }
        // A curve that does reach memory must not be doubted.
        CacheCurveResult honest;
        honest.points = apple_m4;
        honest.levels =
            estimate_cache_levels(honest.points, 123.1, &reached_memory);
        honest.reached_memory = reached_memory;
        honest.memory_latency_ns = 123.1;
        if (!describe_prefetch_doubt(honest).empty()) {
            std::cerr << "the Apple M4 curve was doubted although it reached "
                         "memory\n";
            ok = false;
        }
    }

    // Two little cores of the same MediaTek MT6993 cluster, measured in the
    // same run.  Their curves are nearly identical, and the point of this
    // case is that the estimator must say the same thing about both: the
    // plateau between 112 KiB and 320 KiB drifts by 1.099 against a 1.10
    // band, so the rule that used that band found three levels on one core
    // and two on the other, and flipped between them from run to run.
    static const MeasuredPoint kMediaTekCpu0[] = {{4, 1.486}, {5, 1.487},
        {6, 1.479}, {7, 1.486}, {8, 1.485}, {10, 1.484}, {12, 1.486},
        {14, 1.483}, {16, 1.484}, {20, 1.485}, {24, 1.485}, {28, 1.485},
        {32, 1.486}, {40, 1.487}, {48, 1.486}, {56, 1.489}, {64, 1.485},
        {80, 2.355}, {96, 2.698}, {112, 2.918}, {128, 3.071}, {160, 3.198},
        {192, 3.272}, {224, 3.306}, {256, 3.650}, {320, 4.489}, {384, 4.978},
        {448, 5.040}, {512, 5.266}, {640, 5.875}, {768, 6.268}, {896, 6.758},
        {1024, 6.656}, {1280, 6.879}, {1536, 6.561}, {1792, 6.484},
        {2048, 6.535}, {2560, 6.680}, {3072, 6.703}, {3584, 6.729},
        {4096, 6.855}, {5120, 6.860}, {6144, 7.268}, {7168, 7.283},
        {8192, 7.102}, {10240, 7.597}, {12288, 6.718}, {14336, 9.160},
        {16384, 10.390}, {20480, 12.053}, {24576, 15.622}, {28672, 17.091},
        {32768, 17.979}, {40960, 20.103}, {49152, 21.714}, {57344, 22.642},
        {65536, 24.030}, {81920, 24.557}, {98304, 24.240}, {114688, 24.948},
        {131072, 25.285}};
    static const MeasuredPoint kMediaTekCpu1[] = {{4, 1.485}, {5, 1.485},
        {6, 1.486}, {7, 1.483}, {8, 1.486}, {10, 1.486}, {12, 1.483},
        {14, 1.484}, {16, 1.485}, {20, 1.483}, {24, 1.486}, {28, 1.483},
        {32, 1.487}, {40, 1.485}, {48, 1.486}, {56, 1.484}, {64, 1.488},
        {80, 2.353}, {96, 2.692}, {112, 2.913}, {128, 3.070}, {160, 3.197},
        {192, 3.264}, {224, 3.297}, {256, 3.334}, {320, 3.374}, {384, 3.783},
        {448, 4.640}, {512, 5.243}, {640, 6.171}, {768, 6.680}, {896, 6.728},
        {1024, 6.563}, {1280, 6.821}, {1536, 6.419}, {1792, 6.519},
        {2048, 6.611}, {2560, 6.627}, {3072, 6.663}, {3584, 6.772},
        {4096, 6.777}, {5120, 6.792}, {6144, 6.911}, {7168, 7.012},
        {8192, 7.305}, {10240, 7.780}, {12288, 6.833}, {14336, 8.407},
        {16384, 9.999}, {20480, 14.407}, {24576, 14.576}, {28672, 16.401},
        {32768, 27.154}, {40960, 20.274}, {49152, 21.051}, {57344, 23.431},
        {65536, 24.353}, {81920, 23.746}, {98304, 24.283}, {114688, 26.569},
        {131072, 25.886}};
    {
        const std::vector<CacheLevelEstimate> cpu0 =
            estimate_cache_levels(measured_curve(
                kMediaTekCpu0, sizeof(kMediaTekCpu0) / sizeof(*kMediaTekCpu0)));
        const std::vector<CacheLevelEstimate> cpu1 =
            estimate_cache_levels(measured_curve(
                kMediaTekCpu1, sizeof(kMediaTekCpu1) / sizeof(*kMediaTekCpu1)));
        if (cpu0.size() != cpu1.size() || cpu0.size() != 3) {
            std::cerr << "two cores of one cluster were read differently: "
                      << cpu0.size() << " levels against " << cpu1.size()
                      << ", expected 3 each\n";
            ok = false;
        }
        for (size_t i = 0; i < cpu0.size() && i < cpu1.size(); ++i) {
            const double ratio = static_cast<double>(cpu0[i].capacity_bytes) /
                static_cast<double>(cpu1[i].capacity_bytes);
            if (ratio < 0.5 || ratio > 2.0) {
                std::cerr << "MT6993 " << cpu0[i].level << " differs between "
                          << "two cores of one cluster: "
                          << cpu0[i].capacity_bytes / kKiB << " KiB against "
                          << cpu1[i].capacity_bytes / kKiB << " KiB\n";
                ok = false;
            }
        }
    }

    // A HiSilicon Kunpeng 920 (64 KiB L1, 512 KiB L2, 32 MiB L3 shared by
    // eight cores).  Its L3 is not one plateau: the latency settles near
    // 13.6 ns between 1.25 and 2.5 MiB, climbs again, and settles near 36 ns
    // between 12 and 16 MiB before memory at 84 ns, which is what a sliced
    // last level looks like from one core.  The curve therefore resolves
    // four levels, and the deepest is the 32 MiB the OS reports.  Reporting
    // the third one as "L3" made the row read "DISAGREES with OS, 0.12x"
    // about a curve that had placed the real boundary exactly.
    static const MeasuredPoint kKunpeng920[] = {{4, 1.924}, {5, 1.924},
        {6, 1.924}, {7, 1.924}, {8, 1.924}, {10, 1.924}, {12, 1.924},
        {14, 1.924}, {16, 1.924}, {20, 1.924}, {24, 1.924}, {28, 1.924},
        {32, 1.924}, {40, 1.924}, {48, 1.924}, {56, 1.924}, {64, 1.924},
        {80, 2.932}, {96, 3.309}, {112, 3.480}, {128, 3.583}, {160, 3.693},
        {192, 3.739}, {224, 3.766}, {256, 3.783}, {320, 3.803}, {384, 3.815},
        {448, 3.821}, {512, 3.836}, {640, 7.815}, {768, 9.866}, {896, 11.072},
        {1024, 11.806}, {1280, 12.738}, {1536, 13.322}, {1792, 13.589},
        {2048, 13.763}, {2560, 13.946}, {3072, 14.180}, {3584, 14.900},
        {4096, 16.811}, {5120, 21.810}, {6144, 25.238}, {7168, 27.693},
        {8192, 29.626}, {10240, 32.491}, {12288, 34.880}, {14336, 36.093},
        {16384, 37.335}, {20480, 39.138}, {24576, 40.652}, {28672, 41.900},
        {32768, 49.241}, {40960, 65.605}, {49152, 73.861}, {57344, 77.900},
        {65536, 79.977}, {81920, 82.162}, {98304, 83.124}, {114688, 83.712},
        {131072, 84.105}};
    {
        const std::vector<CacheLevelEstimate> levels = estimate_cache_levels(
            measured_curve(
                kKunpeng920, sizeof(kKunpeng920) / sizeof(*kKunpeng920)),
            84.1, &reached_memory);
        if (levels.size() != 4 || levels[0].capacity_bytes != 64 * kKiB ||
            levels[1].capacity_bytes != 512 * kKiB ||
            levels.back().capacity_bytes != 32 * 1024 * kKiB) {
            std::cerr << "Kunpeng 920: expected four levels ending at 32 MiB, "
                      << "got " << levels.size() << ":";
            for (const CacheLevelEstimate &level : levels)
                std::cerr << ' ' << level.capacity_bytes / kKiB << " KiB";
            std::cerr << '\n';
            ok = false;
        }
        if (!reached_memory) {
            std::cerr << "Kunpeng 920: a curve ending at 84 ns against an "
                         "84 ns reference did not reach memory\n";
            ok = false;
        }
    }

    // A cache hierarchy cannot answer a larger working set faster.  When the
    // curve falls anyway, something outside the sweep is answering and the
    // capacities are its, not the cache's.  The far-end test alone misses
    // this: on an MT6993 big core the deepest point reached 146 ns against a
    // 156 ns reference, which clears it, while the middle of the curve
    // collapsed and the L1 read 256 KiB for a 64 KiB cache.
    {
        struct Case
        {
            const char *name;
            const MeasuredPoint *points;
            size_t count;
            double memory_ns;
            bool expect_doubt;
        };
        const Case cases[] = {
            {"Kunpeng 920", kKunpeng920,
                sizeof(kKunpeng920) / sizeof(*kKunpeng920), 84.1, false},
            {"Apple M4 Pro", kAppleM4Pro,
                sizeof(kAppleM4Pro) / sizeof(*kAppleM4Pro), 123.1, false},
            {"Kunpeng 920F", kKunpeng920F,
                sizeof(kKunpeng920F) / sizeof(*kKunpeng920F), 135.0, false},
            {"MT6993 cpu0", kMediaTekCpu0,
                sizeof(kMediaTekCpu0) / sizeof(*kMediaTekCpu0), 212.0, true},
            {"MT6993 cpu1", kMediaTekCpu1,
                sizeof(kMediaTekCpu1) / sizeof(*kMediaTekCpu1), 212.0, true},
        };
        for (const Case &item : cases) {
            CacheCurveResult curve;
            curve.points = measured_curve(item.points, item.count);
            curve.memory_latency_ns = item.memory_ns;
            curve.levels = estimate_cache_levels(
                curve.points, item.memory_ns, &reached_memory);
            curve.reached_memory = reached_memory;
            const bool doubted = !describe_prefetch_doubt(curve).empty();
            if (doubted != item.expect_doubt) {
                std::cerr << item.name << ": "
                          << (doubted ? "doubted but should not be"
                                      : "not doubted but should be")
                          << '\n';
                ok = false;
            }
        }
    }

    ok &= expect_levels("flat curve", make_curve(2.0, {}), {});

    // Bandwidth worksets live in a level but not in the one below it.
    if (cpufb::cache_level_workset(0, 32 * kKiB) != 16 * kKiB ||
        cpufb::cache_level_workset(32 * kKiB, 512 * kKiB) != 128 * kKiB ||
        cpufb::cache_level_workset(0, 0) != 0 ||
        cpufb::cache_level_workset(64 * kKiB, 32 * kKiB) != 16 * kKiB) {
        std::cerr << "cache_level_workset placed a working set wrongly\n";
        ok = false;
    }

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
