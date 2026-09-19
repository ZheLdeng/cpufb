#include <stdio.h>
#include "runtime_features.h"

int main()
{
    struct cpufb_x86_runtime_features features =
        cpufb_x86_detect_runtime_features();

    if (features.amx_tile) {
        if (features.amx_int8) {
            printf("_AMX_INT8_\n");
        }
        if (features.amx_bf16) {
            printf("_AMX_BF16_\n");
        }
    }
    if (features.avx_vnni) {
        printf("_AVX_VNNI_\n");
    }
    if (features.avx2) {
        printf("_AVX2_\n");
    }
    if (features.avx512_ifma) {
        printf("_AVX512_IFMA_\n");
    }
    if (features.avx512_vbmi) {
        printf("_AVX512_VBMI_\n");
    }
    if (features.avx512_vpopcntdq) {
        printf("_AVX512_VPOPCNTDQ_\n");
    }
    if (features.avx_vnni_int8) {
        printf("_AVX_VNNI_INT8_\n");
    }
    if (features.avx512_vnni) {
        printf("_AVX512_VNNI_\n");
    }
    if (features.avx512_bf16) {
        printf("_AVX512_BF16_\n");
    }
    if (features.avx512_fp16) {
        printf("_AVX512_FP16_\n");
    }
    if (features.avx512f) {
        printf("_AVX512F_\n");
    }
    if (features.fma) {
        printf("_FMA_\n");
    }
    if (features.avx) {
        printf("_AVX_\n");
    }
    if (features.sse) {
        printf("_SSE_\n");
    }
    if (features.sse2) {
        printf("_SSE2_\n");
    }
    printf("_LDP_\n");
    printf("_ISSUE_\n");
    return 0;
}
