# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `develop` @ the commit after `6ec50c3` (this checklist's
  own commit). Test `develop`; `main` has no tests.
- **Previous report:** `CPUFB_TEST_REPORT.md` of 2026-10-29 against
  `6ec50c3`. One red light (M4 `ctest` 15/16) and six observations; all
  answered below.
- **Since `6ec50c3`:** the empty `... anchored` row reads `not available`
  with its reason, so the CLI test accepts it; the write-allocate verdict
  below 1.25x is one text (fetch or store path, no longer two that a 3%
  drift could flip between); README and the AMX comments record that an M4
  Pro executes AMX. No kernel changed.

## Answers to the previous report

| Report item | Answer |
|---|---|
| 1.1: `cpufb_cli_issue_probe` fails on the M4 because `SME + streaming SVE store anchored` prints `-` / `none` | The row now carries `not available: no ratio keeps the anchor within 97% of its pure rate (the companion's own path is the limit)` in its verdict, which is what the test accepts for a row without an IPC. Please confirm 16/16. |
| 1.2: 482 issue kernels, not 372 | Correct: the FP64 SME classes (`sme_fmopa_f64`, `ssve_fmla_f64`, `ssve_ld1d`, two pairs, one triple) came in with `bbf26c1`; 482 is the current count on an SME machine without SVE. |
| 1.3: AMX executes on the M4 Pro (`AMX : version 0`, fma32.mat 1.94 TFLOPS, fma16.mat 3.96 TFLOPS, ldx 64/128/256 B per instruction) | Accepted as the finding: the M4 still executes the AMX encodings next to SME. README and the kernel comments now say M1 to M4. The IPC of 0.85 for eight instructions per loop means about 9.4 cycles per loop. Nothing further to run; the fma16 at twice fma32 and matint i8 at twice fma16 are the expected shapes. |
| 1.4: strides 3, 4 and 6 hide almost nothing on the M4, 1, 2, 8, 16, 32 do | Both hypotheses in the last checklist are ruled out; the table is the result. The M4's stride detector follows 1 and 2 lines and strides from 8 lines up, not 3 to 6. Recorded as a core property; no probe change. |
| 1.5: `Write allocate` read 0.95x and fell back to `fetch`; the ratio drifts 0.66 / 0.83 / 0.95 | The two low-ratio verdicts are merged: anything up to 1.25x reads `full-line stores gain nothing over partial-line ones: the line is fetched before writing or the store path is the limit`. Stable on the M4 whichever of its three ratios comes up; `skip the line fill` still needs 1.6x. |
| 2.x: X60 3/3, `L2 ways` 14/13/14, rvv rows, `RVV compute:load` 0.398 | As expected. The load rows are the X60's first baseline; nothing to compare yet. |
| 3.1: MT6993 cpu0 `NEON compute:load` 3.23 / 3.13 with `max at` 1:4 and 5:3 | The 97% band spans 1:6 to 5:3 on that core, so the frontier is flat and the point the search names is arbitrary inside it; the band, not the point, is the result. Not a regression; listed under known gaps. |
| 3.2: cpu4 L1 64 KiB, cpu7 128 KiB (second time) | Three clusters on record; the prime core's 128 KiB set-conflict reading stands. |
| 3.2 / 3.3: cpu7 curve and write streams not comparable across rounds (L2 24576 -> 1536 KiB, memory reference 250 -> 119 ns, `Write allocate` flipped) | No PMU on cpu7 and a prefetcher that bends its curve; cross-round comparison on that core is out. Known gap. |

## Apple M4 Pro (macOS)

```bash
cmake --preset macos-arm64 && cmake --build --preset macos-arm64 -j8
cd build/macos-arm64 && ctest
./cpufb '--thread_pool=[0]' --include-test=multi_issue --save=issue.csv | grep anchored
./cpufb '--thread_pool=[0]' --mode=cache | grep 'Write allocate'
```

1. **`ctest` 16/16.** The only change that matters here.
2. The `SME + streaming SVE store anchored` row: IPC `-`, Frontier `none`,
   Verdict starting `not available:`.
3. `Write allocate` reads `full-line stores gain nothing over partial-line
   ones ...` with whatever ratio the run gives (0.66x to 0.95x so far).

Nothing else on the M4 this round; AMX, strides and the streaming-store
note are recorded.

## Milk-V X60, Android MT6993, Graviton3: nothing to run

Their items are closed or recorded as gaps. A `ctest` on `develop` is
welcome on Graviton3 and the X60.

## Known gaps, please do not re-report these

| Gap | Status |
|---|---|
| M4 strides 3, 4, 6 hide little latency while 1, 2, 8+ do | Core property of the M4's stride detector; the table shows it. |
| M4 `Write allocate` ratio drifts 0.66x to 0.95x between runs | One verdict covers the whole range below 1.25x. |
| M4 `ssve_st1w` 0.05 to 0.12 across runs; MT6993 prime `st1w` 1.30 | Streaming-mode stores are bounded off the issue ports; the pair row says so and its anchored row is `not available`. |
| MT6993 cpu0 `NEON compute:load` names 1:4 one run and 5:3 the next | The 97% band is flat from 1:6 to 5:3; the band is the result. |
| MT6993 cpu7: no PMU, prefetcher-bent curve, cross-round numbers not comparable | Compare within one run only. |
| X60 `L2 ways` 13 or 14 against 16 | Reproducible, cause unknown; the row prints it, the CLI test tolerates it. |
| riscv64 has no memory read/write rows and no issue probe | No RVV stream kernels yet; deliberate subset. |
| Cycle column and IPCs move with the clock estimate on hosts without a PMU | Expected; the header names the clock. |
| Prefetch-distance ranking differs per core | Core property, confirmed on four designs. |
| Stream restart cost fit `C` while the points are not exactly `T + C/S` | The fit is a summary; the 10% line is read off the measured segments. |
| One-point 97% band when two independent classes have equal peaks | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Graviton3 L3 capacity reads 8 to 16 MiB or `none` across runs | Sliced last level seen from one core. |
| L3 ways and line; L2 ways on hashed L2 or without huge pages | Not measured, never guessed. |

## Reporting

State the commit tested and the branch (`develop`). For the M4, the
`ctest` summary line and the two grep outputs above.
