#include "memory_bandwidth.hpp"

#include "load.hpp"
#if defined(__linux__) && !defined(__APPLE__)
#include "../runtime_features.hpp"
#endif

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

} // namespace cpufb
