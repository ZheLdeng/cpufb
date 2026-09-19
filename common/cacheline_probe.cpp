#include "cacheline_probe.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <numeric>
#include <random>
#include <vector>

namespace cpufb {

namespace {

const int kFallbackCacheline = 64;
const size_t kProbeBytes = 256 * 1024;
const size_t kMinimumWindowPitch = 256;
const size_t kFlushGranularity = 16;
const int kProbeRepeats = 31;
const double kMinimumBoundaryGain = 1.50;
const double kMinimumBoundaryDelta = 0.10;

volatile uintptr_t g_cacheline_probe_sink = 0;

double elapsed_seconds(const timespec &start, const timespec &end)
{
    return end.tv_sec - start.tv_sec +
        (end.tv_nsec - start.tv_nsec) * 1e-9;
}

double median(std::vector<double> values)
{
    if (values.empty()) return 0.0;

    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    double result = values[middle];
    if ((values.size() & 1U) == 0) {
        std::nth_element(values.begin(), values.begin() + middle - 1,
            values.begin() + middle);
        result = (result + values[middle - 1]) * 0.5;
    }
    return result;
}

uintptr_t *node_at(unsigned char *buffer, size_t window, size_t pitch,
    size_t offset)
{
    return reinterpret_cast<uintptr_t *>(buffer + window * pitch + offset);
}

uintptr_t *make_chain(unsigned char *buffer, const std::vector<size_t> &order,
    size_t pitch, size_t offset)
{
    for (size_t i = 0; i < order.size(); ++i) {
        const size_t next = (i + 1) % order.size();
        *node_at(buffer, order[i], pitch, offset) =
            reinterpret_cast<uintptr_t>(
                node_at(buffer, order[next], pitch, offset));
    }
    return node_at(buffer, order[0], pitch, offset);
}

double time_chain(uintptr_t *next, size_t steps)
{
    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (size_t i = 0; i < steps; ++i)
        next = reinterpret_cast<uintptr_t *>(*next);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);

    g_cacheline_probe_sink ^= reinterpret_cast<uintptr_t>(next);
    return elapsed_seconds(start, end);
}

bool debug_enabled()
{
    const char *value = std::getenv("CPUFB_DEBUG_CACHELINE");
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

} // namespace

int probe_cacheline_size(int theory_cacheline,
    cacheline_flush_fn flush_line,
    cacheline_fence_fn finish_flush)
{
    static const size_t strides[] = {16, 32, 64, 128, 256, 512, 1024};
    const size_t stride_count = sizeof(strides) / sizeof(strides[0]);

    if (flush_line == nullptr || finish_flush == nullptr) return 0;

    void *allocation = nullptr;
    if (posix_memalign(&allocation, 4096, kProbeBytes) != 0) return 0;

    unsigned char *buffer = static_cast<unsigned char *>(allocation);
    std::vector<double> ratios;
    ratios.reserve(stride_count);
    std::mt19937 generator(0x43505546U);

    for (size_t stride_index = 0; stride_index < stride_count; ++stride_index) {
        const size_t stride = strides[stride_index];
        // Windows are spaced four strides apart (and at least four 64-byte
        // lines) so that a forward next-line prefetch triggered by one node
        // can never land on another node.
        const size_t pitch = stride * 4 > kMinimumWindowPitch
            ? stride * 4 : kMinimumWindowPitch;
        const size_t window_count = kProbeBytes / pitch;
        std::vector<size_t> order(window_count);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), generator);

        std::memset(buffer, 0, kProbeBytes);
        // The cold chain sits half a stride ABOVE the reuse chain: both share
        // one line until stride exceeds the line size, and the adjacent-line
        // prefetcher, which only fetches forward, cannot pull the reuse node
        // in.  With the reuse node above the cold node a 64-byte line plus
        // next-line prefetch is indistinguishable from a 128-byte line.
        uintptr_t *first = make_chain(buffer, order, pitch,
            stride + stride / 2);
        uintptr_t *second = make_chain(buffer, order, pitch, stride);

        std::vector<double> samples;
        samples.reserve(kProbeRepeats);
        for (int repeat = 0; repeat < kProbeRepeats; ++repeat) {
            for (size_t offset = 0; offset < kProbeBytes;
                    offset += kFlushGranularity)
                flush_line(buffer + offset);
            finish_flush();

            const double cold_time = time_chain(first, window_count);
            const double reuse_time = time_chain(second, window_count);
            if (cold_time > 0.0 && reuse_time > 0.0)
                samples.push_back(reuse_time / cold_time);
        }
        ratios.push_back(median(samples));
    }

    std::free(allocation);

    size_t boundary = 0;
    double boundary_gain = 0.0;
    double boundary_delta = 0.0;
    for (size_t i = 0; i + 1 < ratios.size(); ++i) {
        if (ratios[i] <= 0.0 || !std::isfinite(ratios[i]) ||
                !std::isfinite(ratios[i + 1]))
            continue;
        const double gain = ratios[i + 1] / ratios[i];
        const double delta = ratios[i + 1] - ratios[i];
        // The cache-line boundary is the first pronounced transition from a
        // reused line to an untouched line. Later strides contain fewer chain
        // nodes and therefore have noisier timing; they must not override an
        // already valid earlier boundary.
        if (gain >= kMinimumBoundaryGain &&
                delta >= kMinimumBoundaryDelta) {
            boundary = i;
            boundary_gain = gain;
            boundary_delta = delta;
            break;
        }
    }

    const int measured = boundary_gain > 0.0
        ? static_cast<int>(strides[boundary])
        : 0;

    // The probe result is reported as measured, even when it disagrees with
    // the OS topology: substituting the reported value here would make the
    // "test" column agree with the "theory" column by construction.  Callers
    // that need a working line size use effective_cacheline_size().

    if (debug_enabled()) {
        std::fprintf(stderr, "cacheline probe ratios:");
        for (size_t i = 0; i < ratios.size(); ++i)
            std::fprintf(stderr, " %zu=%.3f", strides[i], ratios[i]);
        std::fprintf(stderr,
            " boundary=%zu gain=%.3f delta=%.3f measured=%d theory=%d\n",
            strides[boundary], boundary_gain, boundary_delta, measured,
            theory_cacheline);
    }

    return measured;
}

int effective_cacheline_size(int theory_cacheline, int measured_cacheline,
    int fallback_cacheline)
{
    if (theory_cacheline > 0) return theory_cacheline;
    if (measured_cacheline > 0) return measured_cacheline;
    return fallback_cacheline > 0 ? fallback_cacheline : kFallbackCacheline;
}

} // namespace cpufb
