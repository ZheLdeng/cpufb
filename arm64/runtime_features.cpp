#include "runtime_features.hpp"

#include <sys/auxv.h>

#if __has_include(<asm/hwcap.h>)
#include <asm/hwcap.h>
#endif

// Keep cross-builds working with older libc headers.  These values are part of
// the Linux AArch64 userspace ABI (Documentation/arch/arm64/elf_hwcaps.rst).
#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1UL << 1)
#endif
#ifndef HWCAP_ASIMDHP
#define HWCAP_ASIMDHP (1UL << 10)
#endif
#ifndef HWCAP_FCMA
#define HWCAP_FCMA (1UL << 14)
#endif
#ifndef HWCAP_ASIMDDP
#define HWCAP_ASIMDDP (1UL << 20)
#endif
#ifndef HWCAP_SVE
#define HWCAP_SVE (1UL << 22)
#endif
#ifndef HWCAP_ASIMDFHM
#define HWCAP_ASIMDFHM (1UL << 23)
#endif
#ifndef HWCAP2_SVE2
#define HWCAP2_SVE2 (1UL << 1)
#endif
#ifndef HWCAP2_SVEI8MM
#define HWCAP2_SVEI8MM (1UL << 9)
#endif
#ifndef HWCAP2_SVEF32MM
#define HWCAP2_SVEF32MM (1UL << 10)
#endif
#ifndef HWCAP2_SVEF64MM
#define HWCAP2_SVEF64MM (1UL << 11)
#endif
#ifndef HWCAP2_SVEBF16
#define HWCAP2_SVEBF16 (1UL << 12)
#endif
#ifndef HWCAP2_I8MM
#define HWCAP2_I8MM (1UL << 13)
#endif
#ifndef HWCAP2_BF16
#define HWCAP2_BF16 (1UL << 14)
#endif

bool Arm64RuntimeFeatures::supports(const std::string &t) const
{
    if (t == "_ASIMD_" || t == "_LDP_" || t == "_ISSUE_" ||
        t == "_ASIMD_INT_MAC_" || t == "_ASIMD_TBL_") return asimd;
    if (t == "_ASIMD_HP_") return asimd && fp16;
    if (t == "_ASIMD_DP_") return asimd && dotprod;
    if (t == "_BF16_") return asimd && bf16;
    if (t == "_I8MM_") return asimd && i8mm;
    if (t == "_FHM_") return asimd && fp16 && fhm;
    // These assembly files contain both f32 and fp16 entry points.  Requiring
    // FP16 for the whole registration group keeps every listed entry safe.
    if (t == "_ASIMD_FCMA_") return asimd && fp16 && fcma;
    if (t == "_ASIMD_REDUCE_" || t == "_ASIMD_RECIP_") return asimd && fp16;
    if (t == "_SVE_") return sve;
    if (t == "_SVE_I8MM_") return sve && sve_i8mm;
    if (t == "_SVE_BF16_") return sve && sve_bf16;
    if (t == "_SVE_F32MM_") return sve && sve_f32mm;
    if (t == "_SVE_F64MM_") return sve && sve_f64mm;
    if (t == "_SVE_FP16_FMLA_") return sve && fp16;
    if (t == "_SVE2_") return sve && sve2;
    return false;
}

std::vector<std::string> Arm64RuntimeFeatures::runnable_tokens() const
{
    std::vector<std::string> result;
#define CPUFB_ADD_IF_RUNNABLE(token) \
    do { if (supports(#token)) result.emplace_back(#token); } while (false)
#ifdef _ASIMD_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_);
#endif
#ifdef _ASIMD_HP_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_HP_);
#endif
#ifdef _ASIMD_DP_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_DP_);
#endif
#ifdef _BF16_
    CPUFB_ADD_IF_RUNNABLE(_BF16_);
#endif
#ifdef _I8MM_
    CPUFB_ADD_IF_RUNNABLE(_I8MM_);
#endif
#ifdef _FHM_
    CPUFB_ADD_IF_RUNNABLE(_FHM_);
#endif
#ifdef _ASIMD_FCMA_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_FCMA_);
#endif
#ifdef _ASIMD_REDUCE_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_REDUCE_);
#endif
#ifdef _ASIMD_RECIP_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_RECIP_);
#endif
#ifdef _ASIMD_INT_MAC_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_INT_MAC_);
#endif
#ifdef _ASIMD_TBL_
    CPUFB_ADD_IF_RUNNABLE(_ASIMD_TBL_);
#endif
#ifdef _SVE_
    CPUFB_ADD_IF_RUNNABLE(_SVE_);
#endif
#ifdef _SVE_I8MM_
    CPUFB_ADD_IF_RUNNABLE(_SVE_I8MM_);
#endif
#ifdef _SVE_BF16_
    CPUFB_ADD_IF_RUNNABLE(_SVE_BF16_);
#endif
#ifdef _SVE_F32MM_
    CPUFB_ADD_IF_RUNNABLE(_SVE_F32MM_);
#endif
#ifdef _SVE_F64MM_
    CPUFB_ADD_IF_RUNNABLE(_SVE_F64MM_);
#endif
#ifdef _SVE_FP16_FMLA_
    CPUFB_ADD_IF_RUNNABLE(_SVE_FP16_FMLA_);
#endif
#ifdef _SVE2_
    CPUFB_ADD_IF_RUNNABLE(_SVE2_);
#endif
#ifdef _LDP_
    CPUFB_ADD_IF_RUNNABLE(_LDP_);
#endif
#ifdef _ISSUE_
    CPUFB_ADD_IF_RUNNABLE(_ISSUE_);
#endif
#undef CPUFB_ADD_IF_RUNNABLE
    return result;
}

const Arm64RuntimeFeatures &arm64_runtime_features()
{
    static const Arm64RuntimeFeatures features = [] {
        const unsigned long hwcap = getauxval(AT_HWCAP);
        const unsigned long hwcap2 = getauxval(AT_HWCAP2);
        Arm64RuntimeFeatures f;
        f.asimd = hwcap & HWCAP_ASIMD;
        f.fp16 = hwcap & HWCAP_ASIMDHP;
        f.dotprod = hwcap & HWCAP_ASIMDDP;
        f.fcma = hwcap & HWCAP_FCMA;
        f.fhm = hwcap & HWCAP_ASIMDFHM;
        f.sve = hwcap & HWCAP_SVE;
        f.sve2 = hwcap2 & HWCAP2_SVE2;
        f.sve_i8mm = hwcap2 & HWCAP2_SVEI8MM;
        f.sve_f32mm = hwcap2 & HWCAP2_SVEF32MM;
        f.sve_f64mm = hwcap2 & HWCAP2_SVEF64MM;
        f.sve_bf16 = hwcap2 & HWCAP2_SVEBF16;
        f.i8mm = hwcap2 & HWCAP2_I8MM;
        f.bf16 = hwcap2 & HWCAP2_BF16;
        return f;
    }();
    return features;
}
