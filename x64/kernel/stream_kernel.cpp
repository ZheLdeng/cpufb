#include "memory_bandwidth.hpp"

#include "load.hpp"
#include "../runtime_features.h"

namespace cpufb {

StreamKernelSpec select_stream_kernel()
{
    const cpufb_x86_runtime_features features =
        cpufb_x86_detect_runtime_features();
    if (features.avx512f) {
        StreamKernelSpec spec = {
            load_vmovups_zmm_kernel,
            "vmovups.zmm(f32) 512-bit",
            512,
            8
        };
        return spec;
    }
    if (features.avx) {
        StreamKernelSpec spec = {
            load_vmovups_kernel,
            "vmovups.ymm(f32) 256-bit",
            512,
            16
        };
        return spec;
    }
    StreamKernelSpec spec = {
        load_movups_xmm_kernel,
        "movups.xmm(f32) 128-bit",
        512,
        32
    };
    return spec;
}

} // namespace cpufb
