# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `develop` @ the commit after `bbf26c1` (this checklist's
  own commit). Test `develop`: it carries `tests/` and the `ctest`
  registration. `main` is the delivery branch and has neither; building it
  only checks that `cpufb` compiles and runs.
- **Previous report:** `CPUFB_TEST_REPORT.md` of 2026-10-28 against
  `f56b9a8` (M4 Pro, X60, MT6993, Graviton3). Every item it raised is
  answered below, either by a change in this commit or by a request for one
  more measurement. Nothing in it is dismissed.
- **Since `f56b9a8`:** Apple AMX rewritten as assembly kernels with run-time
  gating (M4 only); one load-table layout on every architecture; the
  write-allocate verdict gets a "store-bandwidth bound" outcome; a pair row
  says when one class is bounded off the issue ports; strides 3 and 6 in
  the prefetch table; the CLI test accepts an `L2 ways` reading within a
  quarter of the OS value; `tools/` and `docs/` are no longer tracked.

## Answers to the previous report

| Report item | Answer |
|---|---|
| 1.7-a / 3.5: M4 `ssve_st1w` peak 0.05, MT6993 prime `st1w` 1.30, so `SME + streaming SVE store` says nothing | Real: streaming-mode stores are slow on those cores. The pair row now ends with `<class> peaks at 0.05 alone, so its own path, not the issue ports, bounds this row` whenever one peak is under a fifth of the other. Please confirm the text appears on the M4. |
| 1.7-b: M4 L2 workset, stride 4 hides 0% while 2 and 8 hide 69% and 83% | Unexplained from here; strides 3 and 6 were added to tell "the prefetcher stops at 4" from "stride 4 aliases the set index". Report the whole first table. |
| 1.6: test.md said the M4 L2 workset is 16 MiB | Wrong, the workset is the geometric middle of L1 and L2 (1352 KiB on the M4), 2.4 s is the expected time. Fixed here. |
| 1.7-d: M4 `Write allocate` read `fetch` with the full-line stream at 0.66x of the partial one | A new outcome: `store-bandwidth bound: the full-line stream is slower per line than the partial-line one, so the fill cannot be judged`. The non-temporal row (2.6x) stays the M4 finding. |
| 1.7-e: M4 run-to-run 3-4% | Expected without a PMU (ADD-chain clock); compare within one run. Added to the known gaps. |
| 2.1 / 2.2: X60 `L2 ways` 13 or 14 against 16, `cpufb_cli_cache_probes` fails 2 in 5 | The test now fails only on a gross disagreement (outside 0.75x to 1.25x); the row keeps printing the measured 13 or 14 with `DISAGREES with OS`. Whether 13/14 is the probe or the cache (two ways reserved, or a hashed index on part of the set bits) stays open and is listed as a gap. |
| 2.3: riscv64 has no memory rows at all | Correct: the memory stream kernels exist for ARM64 and x86-64 only. Listed as a gap, not fixed this round. |
| 3.2: MT6993 cpu7 `L1 capacity from set conflicts` 128 KiB (4 x 32 KiB), twice | Treated as a reading, not an error: the set-conflict probe is the one a prefetcher cannot stretch, and it repeated. The old expectation (64 KiB) is withdrawn. One more datum requested below (mid cores). |
| 3.5: MT6993 little cores `NEON compute:load` 4.96 -> 3.21 and `Issue width` 4.97 -> 4.24 with every peak unchanged | Nothing in the pair's kernels or search changed between the two versions; please run `multi_issue` twice on cpu0 and send both `NEON compute:load` rows with their Frontier text, so the ratio the search picked can be compared. |
| 3.5: cpu7 clock estimate 1.104 GHz against 4.21 GHz nominal | Known: the ADD chain is not one add per cycle on every core, and this one has no PMU. All cpu7 IPCs are estimates; listed as a gap. |
| 5: cpu7 FSU 5.08 (earlier report) | Closed by the report itself (4.01). |

## Apple M4 Pro (macOS)

```bash
cmake --preset macos-arm64 && cmake --build --preset macos-arm64 -j8
cd build/macos-arm64 && ctest
./cpufb '--thread_pool=[0]' --mode=cache | tee cache.txt
./cpufb '--thread_pool=[0]' --include-test=multi_issue --save=issue.csv | tee issue.txt
./cpufb '--thread_pool=[0]' --include-test=compute --include-isa=AMX | tee amx.txt
./cpufb '--thread_pool=[0]' --include-test=load | grep -E 'Cache Level|amx' | tee amx-load.txt
```

1. **Build as committed and `ctest` 16/16.** `nm` on `_ISSUE_.S.o` still 372
   kernels (no SVE on the M4).
2. **Apple AMX** (new, never run on hardware). The configure log's
   `compiled ARM64 kernels` either lists `_AMX_` (the detector's child
   process survived `AMX_SET`) or not. The run header then prints
   `AMX : version N` with nine `AMX` compute rows and three `amx-ldx` load
   rows, or `AMX : not available (SIGILL on AMX_SET)` and no rows. The M4
   generation replaced AMX by SME, so **"not available" is the expected
   result and is itself the finding**; rows appearing would mean AMX still
   executes on the M4. If rows appear: IPC should read about 8 per loop over
   the cycles, `amx-fma32.mat` in the hundreds of GFLOPS, `amx-ldx x1/x2/x4`
   at 64/128/256 B per instruction. Send `amx.txt` and `amx-load.txt`
   either way, and the configure log line.
3. **Prefetch table one**, all nine stride rows: whether strides 3 and 6
   hide latency when 4 does not.
4. **`Write allocate`** should now read `store-bandwidth bound` (full-line
   0.66x to 0.83x of partial-line); the non-temporal row stays about 2.5x.
5. **`SME + streaming SVE store`** should carry the new `peaks at 0.0x
   alone ... bounds this row` text.

## Milk-V X60 (RISC-V)

```bash
cmake --preset native-release && cmake --build build/native-release -j8
cd build/native-release
for i in 1 2 3; do ctest -R cpufb_cli_cache_probes; done
for i in 1 2 3; do ./cpufb '--thread_pool=[0]' --include-test=cache --save=c$i.csv >/dev/null; grep 'L2 ways' c$i.csv; done
```

1. `cpufb_cli_cache_probes` passes three times (the criterion changed, not
   the probe).
2. The three `L2 ways` values, as printed: expected 13 or 14 with
   `DISAGREES with OS, 0.8x`, or `not measured: ... did not repeat`. A 16
   would be news.
3. `--include-test=load` now goes through the shared harness: send the L1
   and L2 `rvv-vle8.v` rows and the `RVV compute:load` row of the issue
   table; they should match the last round's numbers within noise.

## Android MT6993 (cpu0, cpu4, cpu7)

```bash
/data/local/tmp/cpufb "--thread_pool=[0]" --include-test=multi_issue --save=issue0a.csv
/data/local/tmp/cpufb "--thread_pool=[0]" --include-test=multi_issue --save=issue0b.csv
/data/local/tmp/cpufb "--thread_pool=[4]" --mode=cache --save=cache4.csv
/data/local/tmp/cpufb "--thread_pool=[7]" --mode=cache --save=cache7.csv
```

1. cpu0 twice: the `NEON compute:load` and `Issue width` rows of both runs
   with their Frontier text (the question is whether 3.21 repeats and at
   which ratio the maximum sits).
2. cpu4 (mid core, never measured): its `L1 capacity from set conflicts`
   and `L1 data cache capacity` rows, next to cpu7's `128 KiB, 4 ways x
   32 KiB`, so the three clusters' L1 sizes are on record together.
3. cpu7: `Write allocate` and the prefetch rows as before; nothing changed
   there.

## Graviton3, Neoverse V3, Kunpeng 920, Kunpeng 920F, Xeon VM: nothing to run

No measurement changed on these; a `ctest` on `develop` is welcome.

## Known gaps, please do not re-report these

| Gap | Status |
|---|---|
| X60 `L2 ways` 13 or 14 against 16 | Reproducible, cause unknown (ways reserved, or a hash on part of the set bits). The row prints it; the CLI test tolerates it. |
| riscv64 has no memory read/write rows and no issue probe | No stream kernels for RVV yet; deliberate subset. |
| Cycle column and IPCs move with the clock estimate on hosts without a PMU (M4, MT6993 cpu7: 1.1 GHz estimate) | Expected; the header names the clock. Compare within one run. |
| Prefetch-distance ranking differs per core (M4 L1 target wins, V3 / Graviton3 L2 target wins, MT6993 mixed) | Core property, confirmed on four designs. |
| Stream restart cost fit `C` while the points are not exactly `T + C/S` | The fit is a summary; the 10% line is read off the measured segments. |
| Streaming-mode SVE stores far below loads (M4 0.05, MT6993 prime 1.30) | Core property; the pair row now says so. |
| One-point 97% band when two independent classes have equal peaks | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Graviton3 L3 capacity reads 8 to 16 MiB or `none` across runs | Sliced last level seen from one core. |
| L3 ways and line; L2 ways on hashed L2 or without huge pages; Android `L2 ways` (no huge pages) | Not measured, never guessed. |

## Reporting

State the commit tested and the branch (`develop`). Send the files named
above; for anything not matching the expected value, the row verbatim.
