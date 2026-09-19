# cpufb

This is a tool for benchmarking insturctions performance of cpu.It can automatically sense the local SIMD|DSA ISAs while compiling.

It includes the following features:

floating-points and AI peak performance, L1 and L2 cache size,
L1 and L2 cache bandwidth, number of ways of L1 cache,
IPC of floating-point instruction, IPC of load instruction,
multiple issue.

* `Multiple issue / cache size / cache bandwidth / multi-way of cache` only for single core. If you test using multiple cores, you will get results from a random core.


## Support OS and ISA

|OS|x86-64|arm64|riscv64|
| ------------ | ------------ | ------------ | ------------ |
|Linux|yes|yes|yes|
|MacOS|no|yes|no|
|Windows|no|no|no|
|Andoroid|no|yes|no|

## Support x86-64 SIMD|DSA ISA

|Arch|ISA|Feature|Data Type|Description|
| ------------ | ------------ | ------------ | ------------ | ------------ |
|SIMD|SSE|Vector|fp32|Before Sandy Bridge|
|SIMD|SSE2|Vector|fp64|Before Sandy Bridge|
|SIMD|AVX|Vector|fp32/fp64|From Sandy Bridge|
|SIMD|FMA|Vector|fp32/fp64|From Haswell/Zen|
|SIMD|AVX512f|Vector|fp32/fp64|From Skylake X/Zen4|
|SIMD|AVX512_VNNI|Vector|int8/int16|From IceLake|
|SIMD|AVX_VNNI|Vector|int8/int16|From Alder Lake|
|SIMD|AVX512_FP16|Vector|fp16|From Intel Sapphire Rapids|
|SIMD|AVX512_BF16|Vector|bf16|From AMD Zen4|
|SIMD|AVX_VNNI_INT8|Vector|int8|Unknown|
|DSA|AMX_INT8|Matrix|int8|From Intel Sapphire Rapids|
|DSA|AMX_BF16|Matrix|bf16|From Intel Sapphire Rapids|

## Support arm64 SIMD ISA

|Arch|ISA|Feature|Data Type|Description|
| ------------ | ------------ | ------------ | ------------ | ------------ |
|SIMD|asimd|Vector|fp32/fp64|From Cortex-A57/A53|
|SIMD|sve|Vector|fp32/fp64|From AWS Graviton3|
|SIMD|asimd_hp|Vector|fp16|From Cortex-A75/A55|
|SIMD|asimd_dp|Vector|int8|From Cortex-A75/A55|
|SIMD|FHM (fp16fml)|Vector|fp32 ← fp16 × fp16|From Cortex-A75/A55, Apple M1+|
|SIMD|bf16|Matrix|bf16|From Cortex-X2/A710/A510|
|SIMD|i8mm|Matrix|int8|From Cortex-X2/A710/A510|
|SIMD|sve_fp16|Vector|fp16|SVE cores with FP16 (Neoverse V1+/Graviton3+)|
|SIMD|sve2|Vector|int16|From Graviton3+/Armv9-A (representative: sqrdmlah)|
|SIMD|sve_f32mm|Matrix|fp32|From Neoverse V1+/Graviton3+/Grace|
|SIMD|sve_f64mm|Matrix|fp64|From Neoverse V1+/Graviton3+/Grace|
|DSA|sme (i8)|Matrix|int8 + mixed-sign (umopa/usmopa/sumopa)|Apple M4, Armv9.0-A SME|
|DSA|sme_f16f16|Matrix|fp16 ← fp16 (fp16 ZA acc)|Armv9.2+ SME-F16F16 (req. SME2)|
|DSA|sme_b16b16|Matrix|bf16 ← bf16 (bf16 ZA acc, non-widening bfmopa)|Armv9.2+ SME-B16B16 (req. SME2)|
|DSA|sme_i16i32|Matrix|int32 ← int16 × int16|Armv9.2+ SME-I16I64 family (req. SME2)|

## Support riscv64 VECTOR ISA

|Arch|ISA|Feature|Data Type|Description|
| ------------ | ------------ | ------------ | ------------ | ------------ |
|Vector|vector|Vector|fp16/fp32/fp64|From RISC-V "V" vector extension. Version 1.0|
|DSA|ime|Matrix|int8|From SpacemiT-X60|
## How to build
---
### Dependencies

* CMake **3.19** or newer (required for `CMakePresets.json`).
* A C/C++ toolchain (gcc/clang) and `make` (or any other CMake generator).
* If you need to benchmark the SVE instruction set, please upgrade GCC to version 8.x.

### Quick start

```sh
# Native build (auto-detects host architecture and SIMD features)
./build.sh                  # release
./build.sh -d               # debug
./build.sh -r               # rebuild from scratch

# Cross-compile for ARM64 (Android-friendly)
./build.sh -a               # configure + build
./build.sh -a --push        # ...and adb push to /data/local/tmp/cpufb
./build.sh -a --run-core 0  # ...and run pinned to core 0
```

The output binary is at `build/<preset>/cpufb`.

### Using CMake directly

`build.sh` is just a thin wrapper around `cmake --preset`. The presets are:

| Preset           | Purpose                                                  |
| ---------------- | -------------------------------------------------------- |
| `native-release` | Build for the host arch (Linux x64, arm64, riscv64; macOS arm64). |
| `native-debug`   | Same, with `CMAKE_BUILD_TYPE=Debug`.                     |
| `macos-arm64`    | Apple Silicon native build.                              |
| `aarch64-cross`  | Cross-compile to `aarch64-linux-gnu` (Android deploy).   |
| `riscv64-cross`  | Cross-compile to `riscv64-linux-gnu`.                    |

```sh
cmake --preset native-release
cmake --build --preset native-release
```

To cross-compile and bypass runtime SIMD detection, pass the feature list:

```sh
cmake --preset aarch64-cross \
    -DCPUFB_SIMD_FEATURES_OVERRIDE="_BF16_;_I8MM_;_SVE_;_SVE2_"
cmake --build --preset aarch64-cross
```

### Cross compile for Android

Use the `aarch64-cross` preset (or `./build.sh -a`). It uses
`cmake/toolchains/aarch64-linux-gnu.cmake` and statically links the binary so
that `/data/local/tmp/cpufb` runs on bare Android. With
`CPUFB_ENABLE_ANDROID_DEPLOY=ON` (the preset turns this on by default), the
extra targets `push_android` / `run_android_core{0,1,7}` are available:

```sh
cmake --build --preset aarch64-cross --target push_android
cmake --build --preset aarch64-cross --target run_android_core0
```

You must ensure that the device can be reached via `adb shell` during build.

### Legacy shell scripts

The old `build_x64.sh`, `build_arm64.sh`, `build_android.sh`,
`build_riscv64.sh` and `clean.sh` have been removed; they no longer listed the
shared sources and could not link. `QUICK_START.md` maps each one to its CMake
preset.

## How to benchmark

`./cpufb --thread_pool=[xxx] --mode=all --idle_time=yyy`

### Memory bandwidth

```sh
./cpufb --thread_pool='[0]' --memory-bandwidth
./cpufb --thread_pool='[0]' --memory-bandwidth --memory-size-mib=1024
# One independent stream per selected CPU; reports aggregate throughput.
./cpufb --thread_pool='[96-127]' --memory-bandwidth --memory-size-mib=64
```

Cycle-normalized columns (IPC, latency, Byte/Cycle) use hardware core cycles
counted with `perf_event_open` around each kernel whenever the PMU is
accessible. Many hosts deny that by default (`perf_event_paranoid >= 3`,
containers, VMs); cpufb then falls back, in this order, and prints the source
in the frequency table's `Counter Source` column plus one warning on stderr:

1. `CPUFB_FREQ_GHZ`, a fixed core frequency supplied by the caller. It is used
   for normalization but never shown as `Test Freq`, because it is not a
   measurement;
2. an `ADD`-chain estimate: one dependent register-register `ADD` retires per
   core cycle on x86-64 and AArch64 alike, so 16 x loops / elapsed is the
   running clock of the pinned core. It needs no privileges and tracks turbo;
3. macOS: a `powermetrics` sample (root); x86-64: the invariant TSC rate;
4. the OS-reported maximum frequency (cpufreq sysfs, `hw.cpufrequency_max`, or
   the Apple Silicon power-manager DVFS table). `Test Freq` stays `-` and the
   source reads `OS-reported frequency (not measured)`.

Estimated sources assume the clock measured by the frequency probe also holds
during the kernel; AVX-512 kernels that lower the core clock therefore read a
little slow (for example 4.6 instead of 4 cycles of FMA latency). Use counted
cycles for publishable IPC/latency numbers:

```bash
CPUFB_FREQ_GHZ=3.3 ./cpufb --thread_pool='[96]' --include-test=load
```

`--memory-bandwidth` measures one independent sequential-read stream per
selected CPU and reports their aggregate throughput.  `--memory-size-mib` is
the workset of each stream. When omitted, cpufb chooses
`max(256 MiB, 4 x detected last-level cache)` per stream and prints the
topology source in the result table. Linux selects the highest data/unified cache from
`/sys/devices/system/cpu/cpuN/cache/index*`.  macOS uses a reported L3 cache
when available; Apple Silicon does not publicly expose its system-level cache,
so it falls back to the largest reported performance-level L2 cache.  Set
`--memory-size-mib` explicitly when a fixed workset is required. The same
size/repetition options can be used with `--include-test=cache` to configure
that table's DRAM row.

### Cache hierarchy and bandwidth

```sh
# Cache capacity/associativity/cache-line probes, plus L3 (when reported) and
# one sequential DRAM-read stream per listed core. Multiple cores are timed
# synchronously and reported as aggregate bandwidth.
./cpufb --thread_pool='[0]' --include-test=cache
# Optional override for the cache table's DRAM row.
./cpufb --thread_pool='[0]' --include-test=cache \
    --memory-size-mib=1024 --memory-repetitions=3

# L1/L2-resident load-instruction bandwidth only; no empirical cache probe.
./cpufb --thread_pool='[0]' --include-test=load
```

The `cache` category owns the empirical pointer-chase capacity and
associativity probes. It also reports a topology-detected L3 capacity and an
L3 sequential-read row when the OS exposes an L3. For multiple selected cores,
the L3 workset is fixed below the shared L3 and split evenly between streams;
the L3 row is omitted once a per-stream share would fit in private L2. The same
table always includes a DRAM sequential-read row. Each selected CPU has its
own DRAM stream whose default workset is `max(256 MiB, 4 x detected
last-level cache)`; with many selected CPUs the per-stream share is reduced so
that the aggregate stays within half of the currently available memory, and
the row is skipped with a warning when even 32 MiB per stream does not fit.
Every stream buffer is first-touched, warmed and measured by the same pinned
worker, so its pages stay on that worker's NUMA node. Keep a multi-core L3 run
within one LLC/NUMA domain. If no L3 is reported (common on Apple Silicon), the
L3 rows are omitted and only the memory row is added. A failed stream is
reported as `not measured` and does not abort the remaining categories.

The empirical probes never consume the OS-reported value they are compared
against, and a probe result is printed as measured even when it disagrees; the
last column only labels the agreement (`probe (agrees with OS)`,
`probe (DISAGREES with OS)`, `probe: not observed`):

- cache-line size: a cold and a reuse pointer chain half a stride apart; the
  cold chain sits above the reuse chain and windows are four strides apart,
  so a forward adjacent-line prefetch cannot make a 64-byte line look like
  128 bytes;
- L1 associativity: a same-set pointer ring against a control ring that
  touches the same pages but different sets, which cancels the DTLB conflicts
  that a bare power-of-two stride otherwise reports as cache ways;
- L1/L2 capacity: the dependent-load latency curve on a fixed quarter-octave
  grid up to 64 MiB; plateaus are detected from the curve alone and each
  capacity is the last working set before the next plateau begins. Without transparent huge pages the ring is shuffled page
  by page so translation misses cannot form a spurious level. The result is
  the effective capacity seen by the pinned core, so a busy SMT sibling or a
  co-tenant lowers it.

The `load` category remains the instruction-level L1/L2 cache-bandwidth table.
It obtains L1/L2 capacity directly from Linux cache-topology sysfs or macOS
cache sysctl, then uses half that capacity (up to 32 MiB) as its bandwidth
workset (x86-64 caps the workset at 2 MiB). A topology failure uses the
empirical probe when the `cache` category ran, otherwise conservative 64 KiB
(L1) and 1 MiB (L2) defaults on ARM64 and no row value on x86-64. The capacity
probe is not run by `--include-test=load`.

`Byte/Cycle` is always per core. For a multi-core ARM64 pool the aggregate
traffic is shown in the `Bandwidth (GB/s)` column (1 GB = 1e9 bytes), with the
worker count. Load buffers are 64-byte (x86-64) or page (ARM64) aligned, the
load kernels run on the pinned pool workers, and each worker keeps the same
buffer slice for warm-up and measurement.

  --thread_pool: [xxx] is the list of cpu thread to benchmarking, from setting affinities. Please reference the result of lstopo command. For example, [0,3,5-8,13-15].

  --idle_time: the interval time(sec) between any two adjacent benchmarks, default is 0.

  --loop_scale: divides every registered benchmark's loop count, for quick smoke
runs; the default is 1.

  --bench_limit: limits how many registered benchmarks are actually run; the
default is 0 for all.

  --mode: `cache` measures the dependent-load latency curve and infers L1/L2
  capacity and latency; `compute` runs compute/IPC benchmarks; `all` runs both
  groups and the existing cache-bandwidth kernels. The default is `all`. An
  explicit `--mode=all` clears any `--include-test` restriction so all five
  output tables are restored; `--exclude-test` is still applied, and the
  discarded `--include-test` names are still validated.

  Unknown options and malformed numeric values are errors; nothing is silently
  ignored.

  --include-test / --exclude-test: comma-separated arm64 or x86-64 benchmark types. Supported types are compute, load, cache, freq, multi_issue.

  --include-isa / --exclude-isa: comma-separated compute ISA names. Examples include asimd, bf16, sve and SME2 on arm64, or avx2, fma, avx512_ifma and amx_bf16 on x86-64.

  --list-categories: list available test categories and compute ISA categories, without requiring --thread_pool.

  --list-instructions: list available compute instruction names for sweep mode, without requiring --thread_pool.

  --sweep-instruction / --scale-instruction: run one compute instruction across the current thread pool prefixes, from 1 core through all bound cores. The value matches the `Core Computation` name; quote it in the shell when it contains parentheses or commas.

  --save / --output: save displayed benchmark data to a compact file. `.csv` paths default to CSV; other paths default to tab-delimited txt.

  --save-format / --output-format: override save format with `csv` or `txt`.

Examples:

```sh
./cpufb --thread_pool=[0] --mode=cache
./cpufb --thread_pool=[0] --mode=compute
./cpufb --thread_pool=[0] --mode=all
./cpufb --list-categories
./cpufb --list-instructions
./cpufb --thread_pool='[0]' --include-test=compute --exclude-isa=sve,SME2
./cpufb --thread_pool='[0]' --exclude-test=load,cache,freq
./cpufb --thread_pool='[0-7]' --sweep-instruction='sve_fmla.vv(f32,f32,f32)'
./cpufb --thread_pool='[0-7]' --sweep-instruction='fmla.vv(f32,f32,f32)' --save=scale.csv
./cpufb --thread_pool='[0]' --include-test=compute --save=compute.txt --save-format=txt
./cpufb --thread_pool='[0]' --include-test=compute --include-isa=avx2,avx512_ifma
./cpufb --thread_pool='[0-1]' --sweep-instruction='MADD52(u64,u52,u52)' --save=ifma-scale.csv
```

The x86-64 path detects and builds only ISA kernels exposed by CPUID. In
addition to the existing SSE, AVX, FMA, VNNI, AVX-512 and AMX tests, it
includes AVX2 integer add/multiply, AVX-512 IFMA, AVX-512 VBMI byte permute,
and AVX-512 VPOPCNTDQ tests when supported. Latency variants are paired with
their throughput rows and are omitted from `--list-instructions`.

See [X86_BENCHMARK_ACCOUNTING.md](X86_BENCHMARK_ACCOUNTING.md) for the audited
operation counts and the exact meaning of the instruction rate and latency
metrics. Remaining follow-up work is tracked in [TODO.md](TODO.md).

### macOS counter backends

On Apple Silicon, `cpufb` first attempts to read per-thread fixed counters
(cycles and retired instructions) through the installed `kperf` framework. The
framework is loaded dynamically, so this path is unavailable rather than a hard
dependency when macOS denies counter access (it usually requires root):

```sh
sudo build/macos-arm64/cpufb '--thread_pool=[0]' --mode=compute
```

Without fixed counters the frequency comes from the unprivileged `ADD`-chain
estimate described above, then from a `powermetrics` sample, and finally from
the nominal maximum in the power manager's IORegistry DVFS tables
(`pmgr/voltage-states*-sram`). That last value is what `Theory Freq` always
shows; cpufb no longer carries a per-model frequency table. When nothing can
be measured, `Test Freq` is `-` and IPC is normalized by the reported value,
labelled `OS-reported frequency (not measured)`. Do not compare estimated
clocks directly with PMU-derived IPC from Linux.

## Experimental pair-issue test

`tools/pair_issue_test.py` measures isolated and joint IPC for two Linux
AArch64 instruction classes without scanning a full ratio grid. It currently
supports SVE FP32 FMLA, SVE `ld1h` as its single default load class, NEON FP32
FMLA, integer scalar ADD, and scalar FP32 FADD. For example:

```sh
./tools/pair_issue_test.py sve-fmla sve-ld1h --core 96
./tools/pair_issue_test.py sve-fmla scalar-add --core 96
./tools/pair_issue_test.py sve-fmla neon-fmla --core 96
./tools/pair_issue_test.py neon-fmla scalar-add --core 96
```

The test starts at normalized pressure points 25%, 50%, and 75%, compares
interleaved and blocked schedules, and adds points only around nonlinear
overlap or a 97%-of-peak knee. See
[PAIR_ISSUE_TEST.md](PAIR_ISSUE_TEST.md) for the measurement equations,
requirements, caveats, JSON output, and dry-run planning mode.

## Compute/load/store issue model

Pass one logical kernel group to the standalone model. Neoverse V3 SVE128
FMLA is the default. The intended BF16 model for the eight-core Neoverse V1
SVE256 machine uses the BFMMLA profile:

```sh
./tools/issue_model.py 1F+2L
./tools/issue_model.py 4F+5L
./tools/issue_model.py 4F+3L+2S
./tools/issue_model.py 4F+3L+2S \
    --profile neoverse-v1-sve256-bfmmla
```

It reports compute/load/store IPC, resource utilization, compute and memory
efficiency, and operations/bytes per cycle. See
[ISSUE_MODEL.md](ISSUE_MODEL.md) for the V3 FMLA, V1 FMLA, and V1 BFMMLA
models, calibration points, measured error budgets, JSON output, and
capacity overrides.

## Automated CLI regression tests

Native x86-64 and ARM64 builds register CTest coverage for category/ISA
filters, invalid filters, exact/ambiguous instruction matching, TXT/CSV
serialization, and single-core instruction-sweep output. The tests validate
CLI behavior and file structure; they intentionally do not compare volatile
performance values.

```sh
cmake --preset native-release
cmake --build --preset native-release
ctest --test-dir build/native-release --output-on-failure
```

Set `CPUFB_TEST_CORE` when the default first allowed CPU is not the desired
test core.


## Some x86-64 CPU benchmark results
---
### Intel Xeon Gold 6455B(2 x 32 x Sapphire Rapids)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| AMX_INT8        | MM(s32,s8,s8)         | 6.3726 Tops      |
| AMX_INT8        | MM(s32,s8,u8)         | 7.5746 Tops      |
| AMX_INT8        | MM(s32,u8,s8)         | 7.5733 Tops      |
| AMX_INT8        | MM(s32,u8,u8)         | 7.5718 Tops      |
| AMX_BF16        | MM(f32,bf16,bf16)     | 3.7868 Tflops    |
| AVX512_VNNI     | DP4A(s32,u8,s8)       | 998.07 Gops      |
| AVX512_VNNI     | DP2A(s32,s16,s16)     | 499.07 Gops      |
| AVX_VNNI        | DP4A(s32,u8,s8)       | 498.96 Gops      |
| AVX_VNNI        | DP2A(s32,s16,s16)     | 249.47 Gops      |
| AVX512_BF16     | DP2A(f32,bf16,bf16)   | 115.16 Gflops    |
| AVX512_FP16     | FMA(f16,f16,f16)      | 499.08 Gflops    |
| AVX512F         | FMA(f32,f32,f32)      | 230.28 Gflops    |
| AVX512F         | FMA(f64,f64,f64)      | 115.17 Gflops    |
| FMA             | FMA(f32,f32,f32)      | 118.35 Gflops    |
| FMA             | FMA(f64,f64,f64)      | 62.385 Gflops    |
| AVX             | ADD(MUL(f32,f32),f32) | 91.59 Gflops     |
| AVX             | ADD(MUL(f64,f64),f64) | 45.85 Gflops     |
| SSE             | ADD(MUL(f32,f32),f32) | 46.493 Gflops    |
| SSE2            | ADD(MUL(f64,f64),f64) | 23.235 Gflops    |
--------------------------------------------------------------
</pre>

For multi-cores:

<pre>
$ ./cpufb --thread_pool=[0-63]
Number Threads: 64
Thread Pool Binding: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| AMX_INT8        | MM(s32,s8,s8)         | 390.67 Tops      |
| AMX_INT8        | MM(s32,s8,u8)         | 380.93 Tops      |
| AMX_INT8        | MM(s32,u8,s8)         | 391.32 Tops      |
| AMX_INT8        | MM(s32,u8,u8)         | 380.28 Tops      |
| AMX_BF16        | MM(f32,bf16,bf16)     | 192.47 Tflops    |
| AVX512_VNNI     | DP4A(s32,u8,s8)       | 48.114 Tops      |
| AVX512_VNNI     | DP2A(s32,s16,s16)     | 24.169 Tops      |
| AVX_VNNI        | DP4A(s32,u8,s8)       | 30.818 Tops      |
| AVX_VNNI        | DP2A(s32,s16,s16)     | 15.74 Tops       |
| AVX512_BF16     | DP2A(f32,bf16,bf16)   | 7.09 Tflops      |
| AVX512_FP16     | FMA(f16,f16,f16)      | 31.473 Tflops    |
| AVX512F         | FMA(f32,f32,f32)      | 14.329 Tflops    |
| AVX512F         | FMA(f64,f64,f64)      | 6.5406 Tflops    |
| FMA             | FMA(f32,f32,f32)      | 7.4039 Tflops    |
| FMA             | FMA(f64,f64,f64)      | 3.9067 Tflops    |
| AVX             | ADD(MUL(f32,f32),f32) | 5.4087 Tflops    |
| AVX             | ADD(MUL(f64,f64),f64) | 2.7339 Tflops    |
| SSE             | ADD(MUL(f32,f32),f32) | 2.9077 Tflops    |
| SSE2            | ADD(MUL(f64,f64),f64) | 1.4791 Tflops    |
--------------------------------------------------------------
</pre>

### AMD Ryzen7 8845HS(8 x Zen4)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| AVX512_VNNI     | DP4A(s32,u8,s8)       | 647.97 GOPS      |
| AVX512_VNNI     | DP2A(s32,s16,s16)     | 324.27 GOPS      |
| AVX512_BF16     | DP2A(f32,bf16,bf16)   | 324.92 GFLOPS    |
| AVX512F         | FMA(f32,f32,f32)      | 163.58 GFLOPS    |
| AVX512F         | FMA(f64,f64,f64)      | 81.786 GFLOPS    |
| FMA             | FMA(f32,f32,f32)      | 163.57 GFLOPS    |
| FMA             | FMA(f64,f64,f64)      | 81.785 GFLOPS    |
| AVX             | ADD(MUL(f32,f32),f32) | 157.36 GFLOPS    |
| AVX             | ADD(MUL(f64,f64),f64) | 79.045 GFLOPS    |
| SSE             | ADD(MUL(f32,f32),f32) | 80.34 GFLOPS     |
| SSE2            | ADD(MUL(f64,f64),f64) | 40.371 GFLOPS    |
--------------------------------------------------------------
</pre>

For multi-cores:

<pre>
$ ./cpufb --thread_pool=[0-7]
Number Threads: 8
Thread Pool Binding: 0 1 2 3 4 5 6 7
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| AVX512_VNNI     | DP4A(s32,u8,s8)       | 5113.8 GOPS      |
| AVX512_VNNI     | DP2A(s32,s16,s16)     | 2559.1 GOPS      |
| AVX512_BF16     | DP2A(f32,bf16,bf16)   | 2551.6 GFLOPS    |
| AVX512F         | FMA(f32,f32,f32)      | 1283.6 GFLOPS    |
| AVX512F         | FMA(f64,f64,f64)      | 641.21 GFLOPS    |
| FMA             | FMA(f32,f32,f32)      | 1271.7 GFLOPS    |
| FMA             | FMA(f64,f64,f64)      | 632.3 GFLOPS     |
| AVX             | ADD(MUL(f32,f32),f32) | 1193.6 GFLOPS    |
| AVX             | ADD(MUL(f64,f64),f64) | 590.85 GFLOPS    |
| SSE             | ADD(MUL(f32,f32),f32) | 613.54 GFLOPS    |
| SSE2            | ADD(MUL(f64,f64),f64) | 307.67 GFLOPS    |
--------------------------------------------------------------
</pre>

### AMD Ryzen9 6900HX(8 x Zen3+)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| FMA             | FMA(f32,f32,f32)      | 151.84 GFLOPS    |
| FMA             | FMA(f64,f64,f64)      | 75.702 GFLOPS    |
| AVX             | ADD(MUL(f32,f32),f32) | 150.86 GFLOPS    |
| AVX             | ADD(MUL(f64,f64),f64) | 75.476 GFLOPS    |
| SSE             | ADD(MUL(f32,f32),f32) | 75.452 GFLOPS    |
| SSE2            | ADD(MUL(f64,f64),f64) | 37.737 GFLOPS    |
--------------------------------------------------------------
</pre>

For multi-cores:

<pre>
$ ./cpufb --thread_pool=[0,2,4,6,8,10,12,14]
Number Threads: 8
Thread Pool Binding: 0 2 4 6 8 10 12 14
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| FMA             | FMA(f32,f32,f32)      | 1057.8 GFLOPS    |
| FMA             | FMA(f64,f64,f64)      | 534.37 GFLOPS    |
| AVX             | ADD(MUL(f32,f32),f32) | 1037.6 GFLOPS    |
| AVX             | ADD(MUL(f64,f64),f64) | 516.21 GFLOPS    |
| SSE             | ADD(MUL(f32,f32),f32) | 518.32 GFLOPS    |
| SSE2            | ADD(MUL(f64,f64),f64) | 258.92 GFLOPS    |
--------------------------------------------------------------
</pre>

### Intel N100(4 x Alder Lake-N)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| AVX_VNNI        | DP4A(s32,u8,s8)       | 108.51 GOPS      |
| AVX_VNNI        | DP2A(s32,s16,s16)     | 54.244 GOPS      |
| FMA             | FMA(f32,f32,f32)      | 54.247 GFLOPS    |
| FMA             | FMA(f64,f64,f64)      | 27.128 GFLOPS    |
| AVX             | ADD(MUL(f32,f32),f32) | 27.128 GFLOPS    |
| AVX             | ADD(MUL(f64,f64),f64) | 13.564 GFLOPS    |
| SSE             | ADD(MUL(f32,f32),f32) | 27.126 GFLOPS    |
| SSE2            | ADD(MUL(f64,f64),f64) | 13.563 GFLOPS    |
--------------------------------------------------------------
</pre>

For multi_cores:

<pre>
$ ./cpufb --thread_pool=[0-3]
Number Threads: 4
Thread Pool Binding: 0 1 2 3
--------------------------------------------------------------
| Instruction Set | Core Computation      | Peak Performance |
| AVX_VNNI        | DP4A(s32,u8,s8)       | 369.66 GOPS      |
| AVX_VNNI        | DP2A(s32,s16,s16)     | 185.09 GOPS      |
| FMA             | FMA(f32,f32,f32)      | 185.08 GFLOPS    |
| FMA             | FMA(f64,f64,f64)      | 92.55 GFLOPS     |
| AVX             | ADD(MUL(f32,f32),f32) | 92.546 GFLOPS    |
| AVX             | ADD(MUL(f64,f64),f64) | 46.269 GFLOPS    |
| SSE             | ADD(MUL(f32,f32),f32) | 92.546 GFLOPS    |
| SSE2            | ADD(MUL(f64,f64),f64) | 46.27 GFLOPS     |
--------------------------------------------------------------
</pre>

## Some arm64 CPU benchmark results

### RaspBerry Pi4(4 x Cortex-A72)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
-------------------------------------------------------------
| Instruction Set | Core Computation     | Peak Performance |
| asimd           | fmla.vs(f32,f32,f32) | 11.958 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32) | 11.958 GFLOPS    |
| asimd           | fmla.vs(f64,f64,f64) | 5.9792 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64) | 5.9792 GFLOPS    |
-------------------------------------------------------------
</pre>

For multi_cores:

<pre>
$ ./cpufb --thread_pool=[0-3]
Number Threads: 4
Thread Pool Binding: 0 1 2 3
-------------------------------------------------------------
| Instruction Set | Core Computation     | Peak Performance |
| asimd           | fmla.vs(f32,f32,f32) | 47.883 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32) | 47.88 GFLOPS     |
| asimd           | fmla.vs(f64,f64,f64) | 23.933 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64) | 23.943 GFLOPS    |
-------------------------------------------------------------
</pre>

### RaspBerry Pi5(4 x Cortex-A76)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
----------------------------------------------------------------
| Instruction Set | Core Computation        | Peak Performance |
| asimd_dp        | dp4a.vs(s32,s8,s8)      | 153.48 GOPS      |
| asimd_dp        | dp4a.vv(s32,s8,s8)      | 153.48 GOPS      |
| asimd_dp        | dp4a.vs(u32,u8,u8)      | 153.47 GOPS      |
| asimd_dp        | dp4a.vv(u32,u8,u8)      | 153.48 GOPS      |
| asimd_hp        | fmla.vs(fp16,fp16,fp16) | 76.738 GFLOPS    |
| asimd_hp        | fmla.vv(fp16,fp16,fp16) | 76.738 GFLOPS    |
| asimd           | fmla.vs(f32,f32,f32)    | 38.369 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32)    | 38.369 GFLOPS    |
| asimd           | fmla.vs(f64,f64,f64)    | 19.185 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64)    | 19.185 GFLOPS    |
----------------------------------------------------------------
</pre>

For multi_cores:

<pre>
$ ./cpufb --thread_pool=[0-3]
Number Threads: 4
Thread Pool Binding: 0 1 2 3
----------------------------------------------------------------
| Instruction Set | Core Computation        | Peak Performance |
| asimd_dp        | dp4a.vs(s32,s8,s8)      | 613.79 GOPS      |
| asimd_dp        | dp4a.vv(s32,s8,s8)      | 614.02 GOPS      |
| asimd_dp        | dp4a.vs(u32,u8,u8)      | 613.98 GOPS      |
| asimd_dp        | dp4a.vv(u32,u8,u8)      | 613.99 GOPS      |
| asimd_hp        | fmla.vs(fp16,fp16,fp16) | 306.88 GFLOPS    |
| asimd_hp        | fmla.vv(fp16,fp16,fp16) | 306.98 GFLOPS    |
| asimd           | fmla.vs(f32,f32,f32)    | 153.48 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32)    | 153.5 GFLOPS     |
| asimd           | fmla.vs(f64,f64,f64)    | 74.513 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64)    | 76.751 GFLOPS    |
----------------------------------------------------------------
</pre>

### Rockchip RK3588(4 x Cortex-A76 + 4 x Cortex-A55)

For single core(Cortex-A55):

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
----------------------------------------------------------------
| Instruction Set | Core Computation        | Peak Performance |
| asimd_dp        | dp4a.vs(s32,s8,s8)      | 58.305 GOPS      |
| asimd_dp        | dp4a.vv(s32,s8,s8)      | 58.311 GOPS      |
| asimd_dp        | dp4a.vs(u32,u8,u8)      | 58.313 GOPS      |
| asimd_dp        | dp4a.vv(u32,u8,u8)      | 58.311 GOPS      |
| asimd_hp        | fmla.vs(fp16,fp16,fp16) | 29.156 GFLOPS    |
| asimd_hp        | fmla.vv(fp16,fp16,fp16) | 29.156 GFLOPS    |
| asimd           | fmla.vs(f32,f32,f32)    | 14.579 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32)    | 14.577 GFLOPS    |
| asimd           | fmla.vs(f64,f64,f64)    | 7.2891 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64)    | 7.2834 GFLOPS    |
----------------------------------------------------------------
</pre>

For multi_cores(Cortex-A55):

<pre>
$ ./cpufb --thread_pool=[0-3]
Number Threads: 4
Thread Pool Binding: 0 1 2 3
----------------------------------------------------------------
| Instruction Set | Core Computation        | Peak Performance |
| asimd_dp        | dp4a.vs(s32,s8,s8)      | 232.58 GOPS      |
| asimd_dp        | dp4a.vv(s32,s8,s8)      | 232.46 GOPS      |
| asimd_dp        | dp4a.vs(u32,u8,u8)      | 232.59 GOPS      |
| asimd_dp        | dp4a.vv(u32,u8,u8)      | 232.54 GOPS      |
| asimd_hp        | fmla.vs(fp16,fp16,fp16) | 116.29 GFLOPS    |
| asimd_hp        | fmla.vv(fp16,fp16,fp16) | 116.28 GFLOPS    |
| asimd           | fmla.vs(f32,f32,f32)    | 58.145 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32)    | 58.14 GFLOPS     |
| asimd           | fmla.vs(f64,f64,f64)    | 29.072 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64)    | 29.07 GFLOPS     |
----------------------------------------------------------------
</pre>

For single core(Cortex-A76):

<pre>
$ ./cpufb --thread_pool=[4]
Number Threads: 1
Thread Pool Binding: 4
----------------------------------------------------------------
| Instruction Set | Core Computation        | Peak Performance |
| asimd_dp        | dp4a.vs(s32,s8,s8)      | 151.74 GOPS      |
| asimd_dp        | dp4a.vv(s32,s8,s8)      | 151.75 GOPS      |
| asimd_dp        | dp4a.vs(u32,u8,u8)      | 151.75 GOPS      |
| asimd_dp        | dp4a.vv(u32,u8,u8)      | 151.74 GOPS      |
| asimd_hp        | fmla.vs(fp16,fp16,fp16) | 75.862 GFLOPS    |
| asimd_hp        | fmla.vv(fp16,fp16,fp16) | 75.862 GFLOPS    |
| asimd           | fmla.vs(f32,f32,f32)    | 37.927 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32)    | 37.925 GFLOPS    |
| asimd           | fmla.vs(f64,f64,f64)    | 18.961 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64)    | 18.961 GFLOPS    |
----------------------------------------------------------------
</pre>

For multi_cores(Cortex-A76):

<pre>
$ ./cpufb --thread_pool=[4-7]
Number Threads: 4
Thread Pool Binding: 4 5 6 7
----------------------------------------------------------------
| Instruction Set | Core Computation        | Peak Performance |
| asimd_dp        | dp4a.vs(s32,s8,s8)      | 599.34 GOPS      |
| asimd_dp        | dp4a.vv(s32,s8,s8)      | 600.34 GOPS      |
| asimd_dp        | dp4a.vs(u32,u8,u8)      | 600.04 GOPS      |
| asimd_dp        | dp4a.vv(u32,u8,u8)      | 598.17 GOPS      |
| asimd_hp        | fmla.vs(fp16,fp16,fp16) | 298.94 GFLOPS    |
| asimd_hp        | fmla.vv(fp16,fp16,fp16) | 298.91 GFLOPS    |
| asimd           | fmla.vs(f32,f32,f32)    | 150 GFLOPS       |
| asimd           | fmla.vv(f32,f32,f32)    | 150.08 GFLOPS    |
| asimd           | fmla.vs(f64,f64,f64)    | 75.046 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64)    | 75.034 GFLOPS    |
----------------------------------------------------------------
</pre>

### Phytium,D2000/8

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
-------------------------------------------------------------
| Instruction Set | Core Computation     | Peak Performance |
| asimd           | fmla.vs(f32,f32,f32) | 18.376 GFLOPS    |
| asimd           | fmla.vv(f32,f32,f32) | 18.375 GFLOPS    |
| asimd           | fmla.vs(f64,f64,f64) | 9.1877 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64) | 9.1891 GFLOPS    |
-------------------------------------------------------------
</pre>

For multi_cores:

<pre>
$ ./cpufb --thread_pool=[0-3]
Number Threads: 4
Thread Pool Binding: 0 1 2 3
-------------------------------------------------------------
| Instruction Set | Core Computation     | Peak Performance |
| asimd           | fmla.vs(f32,f32,f32) | 73.51 GFLOPS     |
| asimd           | fmla.vv(f32,f32,f32) | 73.51 GFLOPS     |
| asimd           | fmla.vs(f64,f64,f64) | 36.755 GFLOPS    |
| asimd           | fmla.vv(f64,f64,f64) | 36.747 GFLOPS    |
-------------------------------------------------------------
</pre>

## Some riscv64 CPU benchmark results

### SpacemiT K1(8 x SpacemiT-X60)

For single core:

<pre>
$ ./cpufb --thread_pool=[0]
Number Threads: 1
Thread Pool Binding: 0
---------------------------------------------------------------
| Instruction Set | Core Computation       | Peak Performance |
| ime             | vmadot(s32,s8,s8)      | 511.53 GOPS      |
| ime             | vmadotu(u32,u8,u8)     | 511.5 GOPS       |
| ime             | vmadotus(s32,u8,s8)    | 511.53 GOPS      |
| ime             | vmadotsu(s32,s8,u8)    | 511.51 GOPS      |
| ime             | vmadotslide(s32,s8,s8) | 511.51 GOPS      |
| vector          | vfmacc.vf(f16,f16,f16) | 66.722 GFLOPS    |
| vector          | vfmacc.vv(f16,f16,f16) | 63.936 GFLOPS    |
| vector          | vfmacc.vf(f32,f32,f32) | 33.36 GFLOPS     |
| vector          | vfmacc.vv(f32,f32,f32) | 31.968 GFLOPS    |
| vector          | vfmacc.vf(f64,f64,f64) | 16.679 GFLOPS    |
| vector          | vfmacc.vv(f64,f64,f64) | 15.985 GFLOPS    |
---------------------------------------------------------------
</pre>

For cluster 0(with ime extension), 4 cores:

<pre>
$ ./cpufb --thread_pool=[0-3]
Number Threads: 4
Thread Pool Binding: 0 1 2 3
---------------------------------------------------------------
| Instruction Set | Core Computation       | Peak Performance |
| ime             | vmadot(s32,s8,s8)      | 2.046 TOPS       |
| ime             | vmadotu(u32,u8,u8)     | 2.0462 TOPS      |
| ime             | vmadotus(s32,u8,s8)    | 2.0461 TOPS      |
| ime             | vmadotsu(s32,s8,u8)    | 2.0462 TOPS      |
| ime             | vmadotslide(s32,s8,s8) | 2.0461 TOPS      |
| vector          | vfmacc.vf(f16,f16,f16) | 266.88 GFLOPS    |
| vector          | vfmacc.vv(f16,f16,f16) | 255.75 GFLOPS    |
| vector          | vfmacc.vf(f32,f32,f32) | 133.43 GFLOPS    |
| vector          | vfmacc.vv(f32,f32,f32) | 127.85 GFLOPS    |
| vector          | vfmacc.vf(f64,f64,f64) | 66.709 GFLOPS    |
| vector          | vfmacc.vv(f64,f64,f64) | 63.935 GFLOPS    |
---------------------------------------------------------------
</pre>

For 2 clusters, 8 cores:

<pre>
$ ./cpufb --thread_pool=[0-7]
Number Threads: 8
Thread Pool Binding: 0 1 2 3 4 5 6 7
---------------------------------------------------------------
| Instruction Set | Core Computation       | Peak Performance |
| vector          | vfmacc.vf(f16,f16,f16) | 533.65 GFLOPS    |
| vector          | vfmacc.vv(f16,f16,f16) | 511.45 GFLOPS    |
| vector          | vfmacc.vf(f32,f32,f32) | 266.89 GFLOPS    |
| vector          | vfmacc.vv(f32,f32,f32) | 255.75 GFLOPS    |
| vector          | vfmacc.vf(f64,f64,f64) | 133.42 GFLOPS    |
| vector          | vfmacc.vv(f64,f64,f64) | 127.86 GFLOPS    |
---------------------------------------------------------------
</pre>
