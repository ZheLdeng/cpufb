#ifndef CPUFB_X64_RUNTIME_FEATURES_H
#define CPUFB_X64_RUNTIME_FEATURES_H

#include <stdint.h>

struct cpufb_x86_runtime_features
{
    int sse;
    int sse2;
    int avx;
    int fma;
    int avx2;
    int avx_vnni;
    int avx_vnni_int8;
    int avx512f;
    int avx512_ifma;
    int avx512_vbmi;
    int avx512_vpopcntdq;
    int avx512_vnni;
    int avx512_bf16;
    int avx512_fp16;
    int amx_tile;
    int amx_int8;
    int amx_bf16;
};

struct cpufb_x86_cpuid
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

static inline void cpufb_x86_cpuid_exec(uint32_t leaf, uint32_t subleaf,
    struct cpufb_x86_cpuid *result)
{
    __asm__ volatile ("cpuid"
        : "=a"(result->eax), "=b"(result->ebx),
          "=c"(result->ecx), "=d"(result->edx)
        : "0"(leaf), "2"(subleaf));
}

static inline uint64_t cpufb_x86_xgetbv(uint32_t index)
{
    uint32_t eax;
    uint32_t edx;
    __asm__ volatile ("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((uint64_t)edx << 32) | eax;
}

static inline struct cpufb_x86_runtime_features
cpufb_x86_detect_runtime_features(void)
{
    struct cpufb_x86_runtime_features features = {0};
    struct cpufb_x86_cpuid basic = {0};
    struct cpufb_x86_cpuid leaf1 = {0};
    struct cpufb_x86_cpuid leaf7_0 = {0};
    struct cpufb_x86_cpuid leaf7_1 = {0};
    uint64_t xcr0 = 0;
    int osxsave;
    int avx_state;
    int avx512_state;
    int amx_state;

    cpufb_x86_cpuid_exec(0, 0, &basic);
    if (basic.eax < 1) return features;

    cpufb_x86_cpuid_exec(1, 0, &leaf1);
    features.sse = (leaf1.edx >> 25) & 1;
    features.sse2 = (leaf1.edx >> 26) & 1;
    osxsave = (leaf1.ecx >> 27) & 1;
    if (osxsave) xcr0 = cpufb_x86_xgetbv(0);

    avx_state = osxsave && (xcr0 & 0x6) == 0x6;
    avx512_state = avx_state && (xcr0 & 0xe0) == 0xe0;
    amx_state = osxsave && (xcr0 & 0x60000) == 0x60000;
    features.avx = avx_state && ((leaf1.ecx >> 28) & 1);
    features.fma = features.avx && ((leaf1.ecx >> 12) & 1);

    if (basic.eax < 7) return features;
    cpufb_x86_cpuid_exec(7, 0, &leaf7_0);
    if (leaf7_0.eax >= 1) cpufb_x86_cpuid_exec(7, 1, &leaf7_1);

    features.avx2 = features.avx && ((leaf7_0.ebx >> 5) & 1);
    features.avx512f = avx512_state && ((leaf7_0.ebx >> 16) & 1);
    features.avx512_ifma = features.avx512f && ((leaf7_0.ebx >> 21) & 1);
    features.avx512_vbmi = features.avx512f && ((leaf7_0.ecx >> 1) & 1);
    features.avx512_vpopcntdq = features.avx512f &&
        ((leaf7_0.ecx >> 14) & 1);
    features.avx512_vnni = features.avx512f && ((leaf7_0.ecx >> 11) & 1);
    features.avx512_fp16 = features.avx512f && ((leaf7_0.edx >> 23) & 1);
    features.avx_vnni = features.avx && ((leaf7_1.eax >> 4) & 1);
    features.avx512_bf16 = features.avx512f && ((leaf7_1.eax >> 5) & 1);
    features.avx_vnni_int8 = features.avx && ((leaf7_1.edx >> 4) & 1);
    features.amx_tile = amx_state && ((leaf7_0.edx >> 24) & 1);
    features.amx_int8 = features.amx_tile && ((leaf7_0.edx >> 25) & 1);
    features.amx_bf16 = features.amx_tile && ((leaf7_0.edx >> 22) & 1);
    return features;
}

#endif
