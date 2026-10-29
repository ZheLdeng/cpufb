#ifndef CPUFB_ARM64_AMX_HPP
#define CPUFB_ARM64_AMX_HPP

#include <cstdint>

// Apple AMX (kernels in asm/_AMX_.S, compiled when the feature list has
// _AMX_, which the macOS detector emits).  The instructions are undocumented
// and absent from some cores, so every row is gated by amx_available(),
// which executes AMX_SET/AMX_CLR under a SIGILL guard.
extern "C"
{
    void amx_probe(void);
    void amx_fma16_mat(int64_t);
    void amx_fma32_mat(int64_t);
    void amx_fma64_mat(int64_t);
    void amx_fma16_vec(int64_t);
    void amx_fma32_vec(int64_t);
    void amx_fma64_vec(int64_t);
    void amx_matint_i8i8(int64_t);
    void amx_matint_i8i16(int64_t);
    void amx_matint_i16i16(int64_t);
    void amx_ldx_1_kernel(float *, int, int64_t);
    void amx_ldx_2_kernel(float *, int, int64_t);
    void amx_ldx_4_kernel(float *, int, int64_t);
}

// True when this core executes AMX instructions; decided once.
bool amx_available();
// hw.optional.amx_version from sysctl, 0 when the key is absent.
int amx_version();

#endif
