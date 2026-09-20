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

// How the probe works
// -------------------
// Two addresses half a stride apart share a cache line as long as the stride
// does not exceed the line size.  For each candidate stride S the probe keeps
// kNodes such pairs, far apart from each other, flushes them, and walks
//   1. the "cold" chain through the first address of every pair (all misses),
//   2. the "reuse" chain through the second address of every pair.
// If the pair shares a line, step 1 already brought the second address in and
// the reuse walk is nearly free; otherwise it misses like the cold walk.  The
// ratio reuse/cold is therefore ~0 up to the line size and ~1 above it, and
// the line size is the last stride before the ratio jumps.
//
// Prefetchers are the complication: a core that fetches the next line after a
// miss makes the reuse walk fast even when the pair spans two lines, so a
// 64-byte line reads as 128 bytes.  Each stride is therefore measured in both
// directions, once with the reuse address above the cold one and once below,
// and the larger ratio counts: a prefetcher that only runs forward cannot
// help the pair whose reuse address lies below, and vice versa.  A core that
// prefetches both neighbours defeats the method; the result is then twice the
// line size, which is why it is reported next to the OS value, not instead.
//
// Making the pairs cold takes two steps, because neither works everywhere.
// The cache-maintenance instruction (clflush, dc civac) removes a line from
// every level, which gives the strongest contrast, but Apple Silicon executes
// dc civac at EL0 without evicting anything.  So the probe also reads an
// eviction buffer far larger than any L1 before each cold walk: that pushes
// the pairs out of L1 by capacity, and an L1 miss against an L1 hit is
// already a 5-10x difference.

namespace cpufb {

namespace {

const int kFallbackCacheline = 64;
const size_t kStrides[] = {16, 32, 64, 128, 256, 512, 1024};
// The same number of pairs for every stride keeps the timed walks equally
// long (512 misses are tens of microseconds, far above clock granularity)
// and their lines within any L1.
const size_t kNodes = 512;
// Pairs sit four strides (at least four 64-byte lines) apart so that no
// prefetch triggered by one pair can land on another.
const size_t kMinimumPitch = 256;
// Unknown line size: flush at the finest granularity a line can have.
const size_t kFlushGranularity = 16;
const int kRepeats = 31;
// Read between repeats to push the pairs out of L1 by capacity.  8 MiB is
// 40 times the largest L1 in use today and costs a fraction of a millisecond.
const size_t kEvictionBytes = 8 * 1024 * 1024;
// A pair that stops sharing a line turns a hit into a miss: the ratio rises
// several-fold.  1.5x and +0.10 reject timing noise without missing that.
const double kMinimumGain = 1.50;
const double kMinimumDelta = 0.10;

volatile uintptr_t g_cacheline_probe_sink = 0;

double median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

// Links the address `offset` bytes into every window into one ring, visited
// in `order`.
uintptr_t *make_chain(unsigned char *buffer, const std::vector<size_t> &order,
    size_t pitch, size_t offset)
{
    auto node = [&](size_t window) {
        return reinterpret_cast<uintptr_t *>(buffer + window * pitch + offset);
    };
    for (size_t i = 0; i < order.size(); ++i)
        *node(order[i]) =
            reinterpret_cast<uintptr_t>(node(order[(i + 1) % order.size()]));
    return node(order[0]);
}

// Reads one byte of every 16 so that whatever the line size, every line of
// the eviction buffer is brought in and displaces what L1 held before.
void evict_by_capacity(const unsigned char *eviction)
{
    unsigned sum = 0;
    for (size_t i = 0; i < kEvictionBytes; i += kFlushGranularity)
        sum += eviction[i];
    g_cacheline_probe_sink ^= sum;
}

double time_chain(uintptr_t *next, size_t steps)
{
    timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (size_t i = 0; i < steps; ++i)
        next = reinterpret_cast<uintptr_t *>(*next);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    g_cacheline_probe_sink ^= reinterpret_cast<uintptr_t>(next);
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
}

// reuse/cold time ratio for one pair per window: the cold address at
// `cold_offset`, the reuse address at `reuse_offset`.
double reuse_ratio(unsigned char *buffer, const unsigned char *eviction,
    size_t pitch, size_t cold_offset, size_t reuse_offset,
    cacheline_flush_fn flush_line, cacheline_fence_fn finish_flush,
    std::mt19937 &generator)
{
    // A random visiting order keeps stride prefetchers off both chains.
    std::vector<size_t> order(kNodes);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), generator);
    uintptr_t *cold = make_chain(buffer, order, pitch, cold_offset);
    uintptr_t *reuse = make_chain(buffer, order, pitch, reuse_offset);

    const size_t low = std::min(cold_offset, reuse_offset);
    const size_t high = std::max(cold_offset, reuse_offset) + sizeof(uintptr_t);
    // CPUFB_CACHELINE_NO_FLUSH=1 emulates a platform whose flush instruction
    // does nothing, to check that capacity eviction alone finds the line.
    const char *no_flush = std::getenv("CPUFB_CACHELINE_NO_FLUSH");
    const bool skip_flush = no_flush != nullptr && no_flush[0] == '1';
    std::vector<double> cold_times, reuse_times;
    for (int repeat = 0; repeat < kRepeats; ++repeat) {
        // Only the pairs need to leave the cache, not the whole buffer.
        for (size_t window = 0; !skip_flush && window < kNodes; ++window)
            for (size_t offset = low; offset < high + kFlushGranularity;
                offset += kFlushGranularity)
                flush_line(buffer + window * pitch + offset);
        finish_flush();
        evict_by_capacity(eviction);
        cold_times.push_back(time_chain(cold, kNodes));
        reuse_times.push_back(time_chain(reuse, kNodes));
    }
    const double cold_time = median(cold_times);
    return cold_time > 0.0 ? median(reuse_times) / cold_time : 0.0;
}

bool debug_enabled()
{
    const char *value = std::getenv("CPUFB_DEBUG_CACHELINE");
    return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

} // namespace

int probe_cacheline_size(int theory_cacheline, cacheline_flush_fn flush_line,
    cacheline_fence_fn finish_flush)
{
    if (flush_line == nullptr || finish_flush == nullptr) return 0;

    const size_t stride_count = sizeof(kStrides) / sizeof(kStrides[0]);
    const size_t max_pitch = kStrides[stride_count - 1] * 4;
    const size_t bytes = kNodes * max_pitch + 4096;
    void *allocation = nullptr;
    if (posix_memalign(&allocation, 4096, bytes) != 0) return 0;
    unsigned char *buffer = static_cast<unsigned char *>(allocation);
    std::memset(buffer, 0, bytes);
    std::vector<unsigned char> eviction(kEvictionBytes, 1);

    std::mt19937 generator(0x43505546U);
    std::vector<double> ratios, below_ratios, above_ratios;
    for (size_t stride : kStrides) {
        const size_t pitch = std::max(stride * 4, kMinimumPitch);
        // Reuse address below the cold one: immune to forward prefetch.
        const double below = reuse_ratio(buffer, eviction.data(), pitch,
            stride + stride / 2, stride, flush_line, finish_flush, generator);
        // Reuse address above the cold one: immune to backward prefetch.
        const double above = reuse_ratio(buffer, eviction.data(), pitch, stride,
            stride + stride / 2, flush_line, finish_flush, generator);
        below_ratios.push_back(below);
        above_ratios.push_back(above);
        ratios.push_back(std::max(below, above));
    }
    std::free(allocation);

    // The line size is the last stride before the first pronounced rise.
    int measured = 0;
    for (size_t i = 0; i + 1 < ratios.size(); ++i) {
        if (ratios[i] <= 0.0 || !std::isfinite(ratios[i + 1])) continue;
        if (ratios[i + 1] / ratios[i] >= kMinimumGain &&
            ratios[i + 1] - ratios[i] >= kMinimumDelta) {
            measured = static_cast<int>(kStrides[i]);
            break;
        }
    }

    if (debug_enabled()) {
        std::fprintf(stderr, "cacheline probe (reuse/cold, below|above):");
        for (size_t i = 0; i < ratios.size(); ++i)
            std::fprintf(stderr, " %zu=%.3f|%.3f", kStrides[i], below_ratios[i],
                above_ratios[i]);
        std::fprintf(
            stderr, " measured=%d theory=%d\n", measured, theory_cacheline);
    }
    // Reported as measured even when it disagrees with the OS: substituting
    // the OS value would make the two columns agree by construction.
    return measured;
}

int effective_cacheline_size(
    int theory_cacheline, int measured_cacheline, int fallback_cacheline)
{
    if (theory_cacheline > 0) return theory_cacheline;
    if (measured_cacheline > 0) return measured_cacheline;
    return fallback_cacheline > 0 ? fallback_cacheline : kFallbackCacheline;
}

} // namespace cpufb
