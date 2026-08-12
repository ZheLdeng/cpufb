// Linux/AArch64 SME BFMOPA + LD1H ratio grid.  No stores are emitted.
//
// Build:
//   gcc -O3 -std=gnu11 -march=armv9.2-a+sme \
//       tmp/sme_bfmopa_ld1h_linux.c -o /tmp/sme_grid
// Run (pin externally for repeatability):
//   taskset -c 0 /tmp/sme_grid [iterations=1000000] [trials=9]
//
// Each statically expanded loop body has 16 BFMOPA instructions and one of
// 4,8,12,16,20,24,27,32,40,48,64 LD1H instructions.  The result is PMU-based
// target IPC = (BFMOPA + LD1H) / CPU cycles.  The source intentionally has no
// ST1W instruction and no wall-clock fallback.

#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#if !defined(__linux__) || !defined(__aarch64__) || !defined(__ARM_FEATURE_SME)
#error "Build this file on Linux/AArch64 with -march=armv9.2-a+sme"
#endif

// Keep older Linux userspace headers usable.
#ifndef HWCAP2_SME
#define HWCAP2_SME (1UL << 23)
#endif
#ifndef HWCAP2_SME_B16F32
#define HWCAP2_SME_B16F32 (1UL << 28)
#endif

enum {
    k_bfmopa_per_block = 16,
    k_default_iterations = 1000000,
    k_default_trials = 9,
    k_input_elements = 2048,
};

typedef void (*kernel_t)(uint64_t, const uint16_t *);

struct ratio_case {
    const char *ratio;
    uint64_t loads_per_block;
    kernel_t kernel;
};

// SMSTART/SMSTOP can alter normal vector state.  Preserve d8..d15, which are
// AAPCS64 callee-saved, around the streaming-mode region.
#define KERNEL_HEAD                                                               \
    "stp d8, d9, [sp, #-64]!\n"                                                  \
    "stp d10, d11, [sp, #16]\n"                                                 \
    "stp d12, d13, [sp, #32]\n"                                                 \
    "stp d14, d15, [sp, #48]\n"                                                 \
    "smstart\n"                                                                 \
    "ptrue p0.s\n"                                                               \
    "ptrue p1.s\n"                                                               \
    "ptrue p2.h\n"                                                               \
    "ld1h { z1.h }, p2/z, [%[in]]\n"                                            \
    "ld1h { z0.h }, p2/z, [%[in]]\n"

#define KERNEL_TAIL                                                               \
    "subs %x[n], %x[n], #1\n"                                                   \
    "b.ne 1b\n"                                                                  \
    "smstop\n"                                                                   \
    "ldp d8, d9, [sp, #0]\n"                                                    \
    "ldp d10, d11, [sp, #16]\n"                                                 \
    "ldp d12, d13, [sp, #32]\n"                                                 \
    "ldp d14, d15, [sp, #48]\n"                                                 \
    "add sp, sp, #64\n"

#define C(ZA) "bfmopa za" #ZA ".s, p0/m, p1/m, z0.h, z1.h\n"
#define L0 "ld1h { z0.h }, p2/z, [%[in]]\n"
#define LX(Z) "ld1h { z" #Z ".h }, p2/z, [%[in]]\n"
#define CL(ZA) C(ZA) L0
#define CLX(ZA, X0) C(ZA) L0 LX(X0)
#define CL2X(ZA, X0, X1) C(ZA) L0 LX(X0) LX(X1)
#define CL3X(ZA, X0, X1, X2) C(ZA) L0 LX(X0) LX(X1) LX(X2)

#define DEFINE_KERNEL(NAME, BODY)                                                \
    __attribute__((noinline)) static void NAME(uint64_t n, const uint16_t *in)  \
    {                                                                             \
        __asm__ volatile(                                                        \
            KERNEL_HEAD                                                           \
            "1:\n"                                                               \
            BODY                                                                  \
            KERNEL_TAIL                                                           \
            : [n] "+&r"(n)                                                       \
            : [in] "r"(in)                                                       \
            : "cc", "memory", "p0", "p1", "p2", "z0", "z1", "z3", "z4",  \
              "z5", "z6", "z7");                                              \
    }

// LD1H:BFMOPA = 4:16 = 0.25:1.
DEFINE_KERNEL(k_l04,
    C(0) C(1) C(2) CL(3)
    C(0) C(1) C(2) CL(3)
    C(0) C(1) C(2) CL(3)
    C(0) C(1) C(2) CL(3))

// 8:16 = 0.5:1.
DEFINE_KERNEL(k_l08,
    C(0) CL(1) C(2) CL(3)
    C(0) CL(1) C(2) CL(3)
    C(0) CL(1) C(2) CL(3)
    C(0) CL(1) C(2) CL(3))

// 12:16 = 0.75:1.
DEFINE_KERNEL(k_l12,
    CL(0) CL(1) CL(2) C(3)
    CL(0) CL(1) CL(2) C(3)
    CL(0) CL(1) CL(2) C(3)
    CL(0) CL(1) CL(2) C(3))

// 16:16 = 1:1.
DEFINE_KERNEL(k_l16,
    CL(0) CL(1) CL(2) CL(3)
    CL(0) CL(1) CL(2) CL(3)
    CL(0) CL(1) CL(2) CL(3)
    CL(0) CL(1) CL(2) CL(3))

// 20:16 = 1.25:1.
DEFINE_KERNEL(k_l20,
    CLX(0, 3) CL(1) CL(2) CL(3)
    CLX(0, 4) CL(1) CL(2) CL(3)
    CLX(0, 5) CL(1) CL(2) CL(3)
    CLX(0, 6) CL(1) CL(2) CL(3))

// 24:16 = 1.5:1.
DEFINE_KERNEL(k_l24,
    CLX(0, 3) CLX(1, 4) CL(2) CL(3)
    CLX(0, 5) CLX(1, 6) CL(2) CL(3)
    CLX(0, 7) CLX(1, 3) CL(2) CL(3)
    CLX(0, 4) CLX(1, 5) CL(2) CL(3))

// 27:16 = 1.6875:1; the 11 extra loads are distributed through the body.
DEFINE_KERNEL(k_l27,
    CL(0)     CLX(1, 3) CLX(2, 4) CL(3)
    CLX(0, 5) CLX(1, 6) CL(2)     CLX(3, 7)
    CLX(0, 3) CL(1)     CLX(2, 4) CLX(3, 5)
    CL(0)     CLX(1, 6) CLX(2, 7) CLX(3, 3))

// 32:16 = 2:1.
DEFINE_KERNEL(k_l32,
    CLX(0, 3) CLX(1, 4) CLX(2, 5) CLX(3, 6)
    CLX(0, 7) CLX(1, 3) CLX(2, 4) CLX(3, 5)
    CLX(0, 6) CLX(1, 7) CLX(2, 3) CLX(3, 4)
    CLX(0, 5) CLX(1, 6) CLX(2, 7) CLX(3, 3))

// 40:16 = 2.5:1.
DEFINE_KERNEL(k_l40,
    CLX(0, 3) CL2X(1, 4, 5)
    CLX(2, 6) CL2X(3, 7, 3)
    CLX(0, 4) CL2X(1, 5, 6)
    CLX(2, 7) CL2X(3, 3, 4)
    CLX(0, 5) CL2X(1, 6, 7)
    CLX(2, 3) CL2X(3, 4, 5)
    CLX(0, 6) CL2X(1, 7, 3)
    CLX(2, 4) CL2X(3, 5, 6))

// 48:16 = 3:1.
DEFINE_KERNEL(k_l48,
    CL2X(0, 3, 4) CL2X(1, 5, 6) CL2X(2, 7, 3) CL2X(3, 4, 5)
    CL2X(0, 6, 7) CL2X(1, 3, 4) CL2X(2, 5, 6) CL2X(3, 7, 3)
    CL2X(0, 4, 5) CL2X(1, 6, 7) CL2X(2, 3, 4) CL2X(3, 5, 6)
    CL2X(0, 7, 3) CL2X(1, 4, 5) CL2X(2, 6, 7) CL2X(3, 3, 4))

// 64:16 = 4:1.
DEFINE_KERNEL(k_l64,
    CL3X(0, 3, 4, 5) CL3X(1, 6, 7, 3)
    CL3X(2, 4, 5, 6) CL3X(3, 7, 3, 4)
    CL3X(0, 5, 6, 7) CL3X(1, 3, 4, 5)
    CL3X(2, 6, 7, 3) CL3X(3, 4, 5, 6)
    CL3X(0, 7, 3, 4) CL3X(1, 5, 6, 7)
    CL3X(2, 3, 4, 5) CL3X(3, 6, 7, 3)
    CL3X(0, 4, 5, 6) CL3X(1, 7, 3, 4)
    CL3X(2, 5, 6, 7) CL3X(3, 3, 4, 5))

#undef DEFINE_KERNEL
#undef CL3X
#undef CL2X
#undef CLX
#undef CL
#undef LX
#undef L0
#undef C
#undef KERNEL_TAIL
#undef KERNEL_HEAD

static const struct ratio_case k_cases[] = {
    {"0.25:1", 4, k_l04}, {"0.5:1", 8, k_l08},   {"0.75:1", 12, k_l12},
    {"1:1", 16, k_l16},   {"1.25:1", 20, k_l20}, {"1.5:1", 24, k_l24},
    {"1.6875:1", 27, k_l27}, {"2:1", 32, k_l32}, {"2.5:1", 40, k_l40},
    {"3:1", 48, k_l48},   {"4:1", 64, k_l64},
};

static int perf_open(uint64_t config, int group_fd, uint64_t read_format)
{
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = config;
    attr.disabled = group_fd < 0;
    // Pin the group through its leader; setting this on a member is unnecessary.
    attr.pinned = group_fd < 0;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.read_format = read_format;
    return (int)syscall(__NR_perf_event_open, &attr, 0, -1, group_fd, 0);
}

static uint64_t parse_u64(const char *text, const char *name)
{
    char *end = NULL;
    errno = 0;
    const unsigned long long value = strtoull(text, &end, 10);
    if (errno || end == text || *end || value == 0) {
        fprintf(stderr, "%s must be a positive integer\n", name);
        exit(2);
    }
    return (uint64_t)value;
}

int main(int argc, char **argv)
{
    const uint64_t iterations = argc > 1 ? parse_u64(argv[1], "iterations")
                                         : k_default_iterations;
    const uint64_t trials = argc > 2 ? parse_u64(argv[2], "trials")
                                     : k_default_trials;
    if (argc > 3 || trials > 1000 ||
        iterations > UINT64_MAX / (k_bfmopa_per_block + 64)) {
        fprintf(stderr, "usage: %s [iterations] [trials]\n", argv[0]);
        return 2;
    }

    const unsigned long hwcap2 = getauxval(AT_HWCAP2);
    if ((hwcap2 & (HWCAP2_SME | HWCAP2_SME_B16F32)) !=
        (HWCAP2_SME | HWCAP2_SME_B16F32)) {
        fprintf(stderr, "SME + SME_B16F32 is unavailable for this Linux process\n");
        return 77;
    }

    int cycles_fd = perf_open(PERF_COUNT_HW_CPU_CYCLES, -1, PERF_FORMAT_GROUP);
    if (cycles_fd < 0) {
        perror("perf_event_open(cycles)");
        return 1;
    }
    int instructions_fd = perf_open(PERF_COUNT_HW_INSTRUCTIONS, cycles_fd, 0);
    if (instructions_fd < 0) {
        perror("perf_event_open(instructions)");
        close(cycles_fd);
        return 1;
    }

    static uint16_t input[k_input_elements] __attribute__((aligned(256)));
    for (size_t i = 0; i < k_input_elements; ++i) input[i] = 0x3f80;

    printf("no-ST1W SME grid; iterations=%" PRIu64 ", trials=%" PRIu64 "\n", iterations, trials);
    printf("%-10s %14s %14s %14s %16s\n",
           "LD1H:BFMOPA", "cycles", "target IPC", "whole IPC", "BFMOPA/cycle");

    for (size_t c = 0; c < sizeof(k_cases) / sizeof(k_cases[0]); ++c) {
        const struct ratio_case *test = &k_cases[c];
        test->kernel(iterations < 10000 ? iterations : 10000, input);  // warmup

        uint64_t best_cycles = UINT64_MAX;
        uint64_t best_instructions = 0;
        for (uint64_t t = 0; t < trials; ++t) {
            if (ioctl(cycles_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) ||
                ioctl(cycles_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP)) {
                perror("perf ioctl(enable/reset)");
                return 1;
            }
            test->kernel(iterations, input);
            if (ioctl(cycles_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP)) {
                perror("perf ioctl(disable)");
                return 1;
            }

            uint64_t values[3] = {0};
            if (read(cycles_fd, values, sizeof(values)) != (ssize_t)sizeof(values) ||
                values[0] != 2) {
                perror("perf read");
                return 1;
            }
            if (values[1] < best_cycles) {
                best_cycles = values[1];
                best_instructions = values[2];
            }
        }

        const double bfmopa = (double)iterations * k_bfmopa_per_block;
        const double target = bfmopa + (double)iterations * test->loads_per_block;
        printf("%-10s %14" PRIu64 " %14.4f %14.4f %16.4f\n",
               test->ratio, best_cycles, target / best_cycles,
               (double)best_instructions / best_cycles, bfmopa / best_cycles);
    }

    close(instructions_fd);
    close(cycles_fd);
    return 0;
}
