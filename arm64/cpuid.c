#include <stdio.h>
#include <stdint.h>
#ifdef __APPLE__
#include	<sys/types.h>
#include	<sys/sysctl.h>
#else
#include <asm/hwcap.h>
#include <sys/auxv.h>
#endif

int get_cpuid()
{
#ifdef __APPLE__
    int64_t ret = 0;
    size_t size = sizeof(ret);
    if (sysctlbyname("hw.optional.arm.FEAT_I8MM", &ret, &size, NULL, 0) == 0 && ret==1) {
        printf("_I8MM_\n");
    }
    if (sysctlbyname("hw.optional.arm.FEAT_BF16", &ret, &size, NULL, 0) == 0 && ret==1) {
        printf("_BF16_\n");
    }
    if (sysctlbyname("hw.optional.arm.FEAT_DotProd", &ret, &size, NULL, 0) == 0 && ret==1) {
        printf("_ASIMD_DP_\n");
    }
    if (sysctlbyname("hw.optional.AdvSIMD_HPFPCvt", &ret, &size, NULL, 0) == 0 && ret==1) {
        printf("_ASIMD_HP_\n");
    }
    if (sysctlbyname("hw.optional.AdvSIMD", &ret, &size, NULL, 0) == 0 && ret==1) {
        printf("_ASIMD_\n");
    }
#else
    uint64_t hwcaps = getauxval(AT_HWCAP);

    #ifdef HWCAP2_I8MM
    if (hwcaps & HWCAP2_I8MM) {
        printf("_I8MM_\n");
    }
    #endif

    #ifdef HWCAP2_BF16
    if (hwcaps & HWCAP2_BF16) {
        printf("_BF16_\n");
    }
    #endif

    #ifdef HWCAP_ASIMDDP
    if (hwcaps & HWCAP_ASIMDDP) {
        printf("_ASIMD_DP_\n");
    }
    #endif

    #ifdef HWCAP_ASIMDHP
    if (hwcaps & HWCAP_ASIMDHP) {
        printf("_ASIMD_HP_\n");
    }
    #endif

    #ifdef HWCAP_ASIMD
    if (hwcaps & HWCAP_ASIMD) {
        printf("_ASIMD_\n");
    }
    #endif

    #ifdef HWCAP_SVE
    if (hwcaps & HWCAP_SVE) {
        printf("_SVE_LD1W_\n");
    }
    #endif
#endif
    printf("_LDP_\n");
    printf("_ISSUE_\n");
    return 0;
}

int main()
{
    get_cpuid();
    return 0;
}

