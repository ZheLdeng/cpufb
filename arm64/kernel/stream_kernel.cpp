#include "memory_bandwidth.hpp"

#include "load.hpp"
#if defined(__linux__) && !defined(__APPLE__)
#include "../runtime_features.hpp"
#endif

#include <ctime>

extern "C" uint64_t clock_add_chain(int64_t looptime, uint64_t addend);

namespace cpufb {

StreamKernelSpec select_stream_kernel()
{
#if defined(_SVE_) && defined(__linux__) && !defined(__APPLE__)
    if (arm64_runtime_features().sve) {
        const std::uint64_t vector_bytes = load_sve_vector_bytes();
        StreamKernelSpec spec = {load_sve_ld1h_kernel,
            "sve-ld1h(f16) " + std::to_string(vector_bytes * 8) + "-bit",
            16 * vector_bytes, 16};
        return spec;
    }
#endif
    StreamKernelSpec spec = {
        load_neon_ld1h_4x1_kernel, "neon-ld1h-4x1(f16)", 256, 16};
    return spec;
}

// Best of three: interrupts and migrations can only lengthen a run.
double estimate_core_clock_hz()
{
    const int64_t loop_time = 10000000;
    volatile uint64_t addend = 3;
    clock_add_chain(loop_time, addend);
    double best = 0.0;
    for (int repeat = 0; repeat < 3; ++repeat) {
        timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        clock_add_chain(loop_time, addend);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double elapsed =
            end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * 1e-9;
        if (elapsed > 0.0 && (best == 0.0 || elapsed < best)) best = elapsed;
    }
    return best > 0.0 ? loop_time * 16.0 / best : 0.0;
}

} // namespace cpufb
