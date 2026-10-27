# TODO

## Next priorities

1. Add an explicit CPU-frequency override for memory-bandwidth runs on hosts
   where `perf_event_paranoid` blocks PMU cycles and cpufreq sysfs data is
   absent. Keep estimated B/cycle and Load IPC visibly distinct from PMU data.
2. Fix the `size_t`/`%d` format warning for `cpu_id` in
   `common/thread_pool.cpp`.

## Repository and future coverage

- Decide whether generated `benchmark_logs/` should be ignored or curated and
  committed as reference results.
- Add dedicated GFNI, VAES, SHA, and VPCLMULQDQ accounting and tests on
  capable x86 hosts.

## Completed locally

- Added an x86 single-core sequential memory-bandwidth backend with runtime
  ZMM/YMM/XMM kernel selection, the same PMU/cpufreq cycle accounting and
  TXT/CSV schema as ARM64, plus cross-architecture CLI regression coverage.
  Runtime benchmark validation is intentionally pending.
- Added `tools/test_arm64_simd_overrides.sh`, an ARM64 override matrix covering
  baseline infrastructure closure, `_SVE_BF16_`/`_SVE2_` to `_SVE_`,
  `_SME2_` to `_SME_`, linked-symbol boundaries, HWCAP filtering, focused CLI
  tests, NEON/SVE memory-kernel selection, and one executable instruction
  sweep per runnable variant. It passes on the 8-core V1 with GCC 12 and the
  192-core V3 with GCC 15; GCC 15 also builds and links the SME2 closure before
  runtime correctly filters SME2 on hardware that does not expose it.
- Added a calibrated compute/load/store issue model for Neoverse V3 SVE128 and
  Neoverse V1 SVE256 FMLA/BFMMLA. It reports per-component IPC, utilization,
  efficiency, traffic, observed model attainment, and JSON output; all 15
  model tests pass.
- Added the ARM64 single-core sequential memory-bandwidth mode with strict
  single-core binding, configurable workset/repetitions, median/min/max
  reporting, PMU-derived B/cycle and Load IPC, and TXT/CSV output. Apple ARM64
  and the 8-core Neoverse V1 pass 7/7 CLI tests; CPU 7 measured 31.056 GB/s,
  12.042 B/cycle, and 0.376 SVE256 LD1H IPC for a 1 GiB workset.
- Moved shared ARM64/x86 CLI parsing, filters, lists, instruction matching,
  sweep scaling, and TXT/CSV output into `common/cli.*`; kept ISA registration
  and measurements behind architecture callbacks, and validated 6/6 CLI tests
  on Apple ARM64, two Linux ARM64 hosts, C8i, and linux-ali.
- Added cross-architecture CTest coverage for CLI lists and filters, invalid
  categories, exact/ambiguous instruction matching, TXT/CSV serialization, and
  single-core sweep output; validated 6/6 on Apple ARM64, two Linux ARM64
  hosts, C8i, and linux-ali.
- Replaced the x86 frequency probe's optional SSE2/FMA/AVX-load dependencies
  with always-built SSE/SSE2 baseline kernels; added reduced `_SSE2_`,
  `_AVX2_`, and `_AVX_VNNI_` build/run regression coverage and validated it on
  C8i and linux-ali.
- Replaced the separate x86 and ARM64 cacheline heuristics with a shared,
  median-based probe; validated direct 64 B detection in 10/10 runs on each of
  the 192-core ARM, 8-core ARM, C8i, and linux-ali P/E-core targets.
- Validated x86 frequency, multi-issue, scalar/XMM/YMM/ZMM load, and cache
  modules on C8i; bounded load traffic and cache probe repetitions so the
  complete module tests finish in practical time.
- Added x86 runtime CPUID/OSXSAVE/XGETBV filtering for AVX and AVX-512, and
  register AMX benchmarks only after tile-data permission is granted.
- Fixed the x86 L1 associativity probe so C8i consistently measures 12 ways
  instead of measuring the 6-way L1 DTLB conflict first.
- Forced VEX encoding for AVX-VNNI latency kernels so they run on
  AVX-VNNI-only CPUs without requiring AVX-512.
