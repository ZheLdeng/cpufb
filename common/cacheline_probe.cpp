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

namespace {

const int kFallbackCacheline = 64;
const size_t kProbeBytes = 64 * 1024;
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

uintptr_t *node_at(unsigned char *buffer, size_t window, size_t stride,
    size_t offset)
{
    return reinterpret_cast<uintptr_t *>(buffer + window * stride + offset);
}

uintptr_t *make_chain(unsigned char *buffer, const std::vector<size_t> &order,
    size_t stride, size_t offset)
{
    for (size_t i = 0; i < order.size(); ++i) {
        const size_t next = (i + 1) % order.size();
        *node_at(buffer, order[i], stride, offset) =
            reinterpret_cast<uintptr_t>(
                node_at(buffer, order[next], stride, offset));
    }
    return node_at(buffer, order[0], stride, offset);
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

int probe_cacheline_size(int theory_cacheline, int fallback_cacheline,
    cacheline_flush_fn flush_line,
    cacheline_fence_fn finish_flush)
{
    static const size_t strides[] = {16, 32, 64, 128, 256, 512, 1024};
    const size_t stride_count = sizeof(strides) / sizeof(strides[0]);

    const int fallback = fallback_cacheline > 0
        ? fallback_cacheline
        : kFallbackCacheline;
    if (flush_line == nullptr || finish_flush == nullptr)
        return theory_cacheline > 0 ? theory_cacheline : fallback;

    void *allocation = nullptr;
    if (posix_memalign(&allocation, 4096, kProbeBytes) != 0)
        return theory_cacheline > 0 ? theory_cacheline : fallback;

    unsigned char *buffer = static_cast<unsigned char *>(allocation);
    std::vector<double> ratios;
    ratios.reserve(stride_count);
    std::mt19937 generator(0x43505546U);

    for (size_t stride_index = 0; stride_index < stride_count; ++stride_index) {
        const size_t stride = strides[stride_index];
        const size_t window_count = kProbeBytes / stride;
        std::vector<size_t> order(window_count);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), generator);

        std::memset(buffer, 0, kProbeBytes);
        uintptr_t *first = make_chain(buffer, order, stride, 0);
        // Half a window keeps the chains distinct at the 16-byte stride while
        // placing both nodes in one line until stride exceeds the line size.
        uintptr_t *second = make_chain(buffer, order, stride, stride / 2);

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

    // OS cache topology is authoritative when exposed. A disagreement means
    // the empirical curve was noisy or the boundary was not observable, so do
    // not publish a transient 16/32-byte result as the measured line size.
    const int selected = theory_cacheline > 0 && measured != theory_cacheline
        ? theory_cacheline
        : (measured > 0 ? measured : fallback);

    if (debug_enabled()) {
        std::fprintf(stderr, "cacheline probe ratios:");
        for (size_t i = 0; i < ratios.size(); ++i)
            std::fprintf(stderr, " %zu=%.3f", strides[i], ratios[i]);
        std::fprintf(stderr,
            " boundary=%zu gain=%.3f delta=%.3f measured=%d theory=%d selected=%d\n",
            strides[boundary], boundary_gain, boundary_delta, measured,
            theory_cacheline, selected);
    }

    return selected;
}
