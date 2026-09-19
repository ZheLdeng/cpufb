# x86 Benchmark Accounting

This document is the source of truth for x86 compute accounting. `comp_pl` is
the number of mathematical operations performed by one outer assembly-loop
iteration. `inst_pl` is the number of benchmark instructions in that same
iteration.

## Formulas

```text
performance = loop_time * comp_pl * threads / elapsed_seconds
IPC = loop_time * inst_pl * threads / sum(per-thread hardware core cycles)
latency_cycles = 1 / dependency_chain_IPC
```

On Linux, each worker reads `PERF_COUNT_HW_CPU_CYCLES` around the assembly
kernel. The instruction table therefore reports core-clock IPC and latency in
core cycles, matching the ARM measurement method and remaining valid when the
processor uses turbo frequencies. If any worker cannot access its hardware
cycle counter, IPC and latency are reported as unavailable rather than being
estimated from the invariant TSC. Running with sufficient perf-event
permissions is required to populate both columns.

Throughput kernels use independent accumulator/register chains. Latency kernels
use one dependency chain while retaining the listed instruction count per loop.
Loop-control instructions are not included in `inst_pl` or `comp_pl`.
For mixed AVX ADD/MUL rows, the latency kernel alternates eight dependent MUL
and eight dependent ADD instructions, so the reported value is the average
dependency latency per instruction for that mixed sequence.

## Frequency-table baseline

The three instruction-rate columns in the separate x86 frequency table use dedicated,
always-built x86-64 baseline kernels. `FSU32` runs 16 independent packed FP32
`addps` instructions per loop, `FSU64` runs 16 independent packed FP64 `addpd`
instructions, and `LSU ldr` runs 16 independent 128-bit `movups` loads. Their
rates remain instructions per invariant-TSC cycle, not mathematical operations
or core-clock IPC. They are frequency diagnostics and are not used to compute
the instruction table's IPC or latency.

These kernels intentionally use legacy SSE/SSE2 only. Their meaning and
availability therefore do not change when optional FMA, AVX, AVX-512, or AMX
kernels are added to or removed from the build.

## Audited compute counts

| ISA / operation | Instructions per loop | Elements per instruction | Operations per element | `comp_pl` |
|---|---:|---:|---:|---:|
| SSE FP32 ADD or MUL | 16 | 4 | 1 | 64 |
| SSE2 FP64 ADD or MUL | 16 | 2 | 1 | 32 |
| AVX FP32 ADD or MUL | 16 | 8 | 1 | 128 |
| AVX FP64 ADD or MUL | 16 | 4 | 1 | 64 |
| AVX mixed ADD/MUL FP32 | 16 | 8 | 1 | 128 |
| AVX mixed ADD/MUL FP64 | 16 | 4 | 1 | 64 |
| FMA FP32 | 16 | 8 | 2 | 256 |
| FMA FP64 | 16 | 4 | 2 | 128 |
| AVX512F FMA FP32 | 16 | 16 | 2 | 512 |
| AVX512F FMA FP64 | 16 | 8 | 2 | 256 |
| AVX2 `vpaddd` or `vpmulld` | 16 | 8 | 1 | 128 |
| AVX-VNNI DP4A | 16 | 8 outputs x 4 pairs | 2 | 1024 |
| AVX-VNNI DP2A | 16 | 8 outputs x 2 pairs | 2 | 512 |
| AVX-VNNI-INT8 DP4A | 16 | 8 outputs x 4 pairs | 2 | 1024 |
| AVX512-VNNI DP4A | 16 | 16 outputs x 4 pairs | 2 | 2048 |
| AVX512-VNNI DP2A | 16 | 16 outputs x 2 pairs | 2 | 1024 |
| AVX512-BF16 DP2A | 16 | 16 outputs x 2 pairs | 2 | 1024 |
| AVX512-FP16 FMA | 16 | 32 | 2 | 1024 |
| AVX512-IFMA `vpmadd52luq` | 16 | 8 | 2 | 256 |
| AVX512-VBMI `vpermb` | 16 | 64 output bytes | 1 element operation | 1024 |
| AVX512-VPOPCNTDQ `vpopcntd` | 16 | 16 dwords | 1 element operation | 256 |
| AMX-INT8 tile dot product | 4 | 256 outputs x 64 pairs | 2 | 131072 |
| AMX-BF16 tile dot product | 4 | 256 outputs x 32 pairs | 2 | 65536 |

For VBMI and VPOPCNTDQ, `OPS` means processed vector elements, not arithmetic
FLOPs. IFMA counts the multiply and accumulator add as two integer operations.
VNNI and AMX dot products likewise count one multiply plus one add per pair.

## Disassembly verification

After a native x86 build, verify macro expansion and dependency chains with:

```sh
tools/validate_x64_accounting.sh build/native-release/cpufb
```

The validator counts benchmark mnemonics in the linked binary and skips ISA
kernels that were not built for the current host. For every registered compute
row, the hot loop must contain exactly `inst_pl` benchmark instructions.
Throughput variants must use independent destinations; the matching `_latency`
symbol must repeatedly write the same destination.

To regression-test the always-built frequency kernels against reduced x86 SIMD
overrides, run this on an x86-64 host:

```sh
tools/test_x64_frequency_overrides.sh
```

It configures, builds, disassembles, and runs the frequency category with
`_SSE2_`, `_AVX2_`, and `_AVX_VNNI_` overrides that deliberately omit FMA.
