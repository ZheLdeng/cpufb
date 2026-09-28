#ifndef CPUFB_ISSUE_LAYOUT_H
#define CPUFB_ISSUE_LAYOUT_H

/* Layout of the issue-probe kernel family, shared by the assembly generator
 * (<arch>/asm/_ISSUE_.S), the kernel tables (<arch>/kernel/issue_kernels.cpp)
 * and the probe logic (common/issue_probe.cpp).  Everything here is plain
 * preprocessor arithmetic so that it reads the same to cpp, GAS and C++.
 *
 * A kernel is void fn(const void *l1_buffer, int64_t loops).  Its body is a
 * fixed instruction stream of one, two or three instruction classes with no
 * data dependency between instructions other than the rotating accumulator
 * chains of each class; see issue_probe.hpp for how the streams are read.
 */

/* Register bank a class draws its rotating registers from. */
#define ISSUE_BANK_GPR 0
#define ISSUE_BANK_VEC 1
#define ISSUE_BANK_ZA 2

/* Processor mode a class needs; classes of different modes never mix. */
#define ISSUE_MODE_NORMAL 0
#define ISSUE_MODE_STREAMING 1

/* Pure kernels: instructions per loop body.  Divisible by every chain count
 * a pure kernel uses (2, 4, 6, 8, 12, 16, 24), so the register rotation
 * restarts exactly at the loop boundary. */
#define ISSUE_PURE_BODY 240

/* Mixed kernels: a pattern of n = a + b (+ c) instructions is repeated this
 * many times per loop body.  A multiple of 16 keeps every class count per
 * body a multiple of its chain count (16 or 8) and the body at or above 256
 * instructions, so loop control stays under one percent. */
#define ISSUE_MIX_REPEATS(n) (16 * ((256 + 16 * (n) - 1) / (16 * (n))))

/* Pair ratio grid: every a:b with a + b <= 8 and gcd(a, b) = 1, i.e. the
 * Farey fractions of order 8 in (0, 1).  The probe searches this grid for
 * the mix that maximises total IPC (issue_probe.cpp); it never runs all of
 * it.  X(A, B, a, b) is expanded once per entry, SEP between entries, so the
 * same list emits assembly (SEP = ;), declarations (SEP = ;) and table
 * initialisers (SEP = ISSUE_COMMA). */
#define ISSUE_COMMA ,
#define ISSUE_RATIO_GRID(X, A, B, SEP)                                        \
    X(A, B, 1, 7) SEP X(A, B, 1, 6) SEP X(A, B, 1, 5) SEP X(A, B, 1, 4) SEP    \
    X(A, B, 1, 3) SEP X(A, B, 2, 5) SEP X(A, B, 1, 2) SEP X(A, B, 3, 5) SEP    \
    X(A, B, 2, 3) SEP X(A, B, 3, 4) SEP X(A, B, 1, 1) SEP X(A, B, 4, 3) SEP    \
    X(A, B, 3, 2) SEP X(A, B, 5, 3) SEP X(A, B, 2, 1) SEP X(A, B, 5, 2) SEP    \
    X(A, B, 3, 1) SEP X(A, B, 4, 1) SEP X(A, B, 5, 1) SEP X(A, B, 6, 1) SEP    \
    X(A, B, 7, 1)

/* Triple grid: every a:b:c with a + b + c <= 6.  The probe evaluates the
 * entries nearest to the peak-proportional mix. */
#define ISSUE_TRIPLE_GRID(X, A, B, C, SEP)                                    \
    X(A, B, C, 1, 1, 1) SEP X(A, B, C, 2, 1, 1) SEP X(A, B, C, 1, 2, 1) SEP    \
    X(A, B, C, 1, 1, 2) SEP X(A, B, C, 3, 1, 1) SEP X(A, B, C, 1, 3, 1) SEP    \
    X(A, B, C, 1, 1, 3) SEP X(A, B, C, 2, 2, 1) SEP X(A, B, C, 2, 1, 2) SEP    \
    X(A, B, C, 1, 2, 2) SEP X(A, B, C, 4, 1, 1) SEP X(A, B, C, 1, 4, 1) SEP    \
    X(A, B, C, 1, 1, 4) SEP X(A, B, C, 3, 2, 1) SEP X(A, B, C, 3, 1, 2) SEP    \
    X(A, B, C, 2, 3, 1) SEP X(A, B, C, 1, 3, 2) SEP X(A, B, C, 2, 1, 3) SEP    \
    X(A, B, C, 1, 2, 3) SEP X(A, B, C, 2, 2, 2)

#endif
