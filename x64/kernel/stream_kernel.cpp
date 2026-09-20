#include "memory_bandwidth.hpp"

#include "load.hpp"
#include "frequency.hpp"
#include "../runtime_features.h"

#include <ctime>

namespace cpufb {

StreamKernelSpec select_stream_kernel()
{
    const cpufb_x86_runtime_features features =
        cpufb_x86_detect_runtime_features();
    if (features.avx512f) {
        StreamKernelSpec spec = {
            load_vmovups_zmm_kernel, "vmovups.zmm(f32) 512-bit", 512, 8};
        return spec;
    }
    if (features.avx) {
        StreamKernelSpec spec = {
            load_vmovups_kernel, "vmovups.ymm(f32) 256-bit", 512, 16};
        return spec;
    }
    StreamKernelSpec spec = {
        load_movups_xmm_kernel, "movups.xmm(f32) 128-bit", 512, 32};
    return spec;
}

// Best of three: interrupts and migrations can only lengthen a run.
double estimate_core_clock_hz()
{
    const int64_t loop_time = 25000000;
    volatile uint64_t addend = 3;
    cpufb_x64_frequency_add_chain(loop_time, addend);
    double best = 0.0;
    for (int repeat = 0; repeat < 3; ++repeat) {
        timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        cpufb_x64_frequency_add_chain(loop_time, addend);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double elapsed =
            end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) * 1e-9;
        if (elapsed > 0.0 && (best == 0.0 || elapsed < best)) best = elapsed;
    }
    return best > 0.0 ? loop_time * 16.0 / best : 0.0;
}

} // namespace cpufb
