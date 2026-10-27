# Experimental pair-issue test

`tools/pair_issue_test.py` measures how two AArch64 instruction classes share
a core. It is deliberately a test feature rather than part of cpufb's stable
benchmark table: it generates a small assembly probe for each selected point,
compiles it locally, and reads hardware counters through Linux `perf`.

The current classes are:

| Name | Instruction | Resource class |
| --- | --- | --- |
| `sve-ld1h` | `ld1h zN.h, p1/z, [x1, ..., MUL VL]` | SVE/L1 load |
| `sve-fmla` | `fmla zN.s, p0/m, zN.s, zN.s` | SVE FP |
| `neon-fmla` | `fmla vN.4s, vN.4s, vN.4s` | NEON FP |
| `scalar-add` | `add xN, xN, #1` | integer scalar ALU |
| `scalar-fadd` | `fadd sN, sN, sN` | scalar FP/SIMD bank |

Scalar integer and scalar floating-point operations are separate because they
need not use the same execution resources. `sve-ld1h` is deliberately the only
load class: the sparse search uses a normal contiguous vector load rather than
duplicating every ratio for `ld1rqh` or other specialized load forms.

## Running a pair

The live probe requires Linux AArch64, a C compiler, `taskset`, permission to
read `cycles:u` and `instructions:u` with `perf`, and SVE when `sve-fmla` is
or `sve-ld1h` is selected.

Some cloud images set `kernel.perf_event_paranoid=4`. If the current account
has deliberately configured passwordless sudo for `perf`, opt in explicitly:

```sh
./tools/pair_issue_test.py sve-fmla scalar-add --core 0 --sudo-perf
```

The default never invokes sudo, and `--sudo-perf` uses `sudo -n` so it fails
instead of prompting or hanging.

```sh
# SVE compute + the default load class
./tools/pair_issue_test.py sve-fmla sve-ld1h --core 96

# SVE + integer scalar
./tools/pair_issue_test.py sve-fmla scalar-add --core 96

# SVE + NEON
./tools/pair_issue_test.py sve-fmla neon-fmla --core 96

# NEON + integer scalar
./tools/pair_issue_test.py neon-fmla scalar-add --core 96
```

Select only the pairs relevant to the kernel rather than forming a rectangular
grid; add one hand-selected multi-class kernel only after the pairwise results
identify the relevant knees.

Useful controls include:

```sh
# Save a machine-readable report.
./tools/pair_issue_test.py sve-fmla scalar-add --core 96 \
    --format json > pair.json

# Plan ratios without compiling or running; pure peaks are supplied manually.
./tools/pair_issue_test.py sve-fmla scalar-add \
    --dry-run --peak-a 4 --peak-b 6

# More precision near a knee, at additional run time.
./tools/pair_issue_test.py sve-fmla scalar-add --core 96 \
    --max-points 9 --tolerance 0.02 --repetitions 5
```

The defaults execute one million loop iterations, repeat every counter sample
three times, and use the median-cycle sample. Every generated loop contains at
least 256 tested instructions, so its `subs`/`b.ne` control overhead is below
one percent of the static loop body. The expansion also makes each class's
per-loop instruction count a whole 16-register rotation period. This prevents
the static loop boundary from prematurely reusing an accumulator.

## Sparse normalized search

First, the tool measures the pure peaks `P_A` and `P_B`. For a pressure share
`alpha`, it chooses a small integer pair close to:

```text
a : b = alpha * P_A : (1 - alpha) * P_B
```

Thus `alpha=0.5` gives equal *normalized* pressure (`a/P_A = b/P_B`), not
necessarily equal instruction counts. Initial mixed points are 0.25, 0.50,
and 0.75. Each point is tested with:

- an evenly interleaved schedule;
- a blocked `AAAA...BBBB...` schedule;
- a phase-shifted interleaver only when the first two differ by more than the
  tolerance (3% by default).

The fastest schedule represents the achievable resource frontier. The tool
then bisects only where either:

- overlap is nonlinear relative to neighboring measured points; or
- A or B crosses the protected efficiency threshold (97% by default).

`--max-points` bounds the number of distinct ratios; it defaults to seven.

## Reported metrics

For a mixed body taking `C` cycles, the tool reports:

```text
IPC_A       = a / C
IPC_B       = b / C
total IPC   = (a + b) / C
efficiencyA = IPC_A / P_A
efficiencyB = IPC_B / P_B
```

It also compares the observed time with two endpoints:

```text
T_serial = a/P_A + b/P_B
T_ideal  = max(a/P_A, b/P_B)
overlap  = (T_serial - C) / (T_serial - T_ideal)
```

An overlap near zero behaves like serial use of one shared resource. An
overlap near one means the two pure rates overlap nearly ideally. Values
outside `[0, 1]` are intentionally not clamped: they expose counter noise,
frequency/run-to-run effects, or a pure baseline that did not reach its true
peak.

`total_body_ipc` excludes the generated loop-control instructions. The report
also includes PMU retired IPC and a retired-count ratio, which checks that the
static body plus two loop instructions agrees with `instructions:u`.

## Dependency and register safeguards

Each instruction class rotates over independent accumulator or destination
chains. The pure test compares half of the available chains with the full set;
a change greater than 2% is reported because the pure peak has not demonstrated
a stable dependency/rename plateau.

SVE `Zn` and NEON `Vn` names alias the same architectural registers. When both
classes use that bank, A receives registers 0-15 and B receives 16-31. This
also applies to scalar FP, whose `Sn` register aliases `Vn`/`Zn`. Integer
scalar ADD uses independent caller-saved general-purpose registers.

The `sve-ld1h` probe rotates over disjoint destination registers and eight
vector-length-scaled offsets in an aligned 4 KiB buffer. The driver touches the
buffer before measurement and the warm-up repeatedly loads it, keeping the
test focused on L1 issue throughput. Loads are intentionally independent of
the compute operands, so there is no load-to-use or cross-class dependency.
A real kernel with address, load-to-use, branch, or accumulator dependencies
should be validated separately at the ratio nearest the measured knee.
