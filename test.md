# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Code structure round (2026-10-27, commits `9c3017c`..`e601013` and this
  one):** no measurement changed, but the output did in three places that a
  re-test should not report as regressions: x86-64 cache rows now carry the
  ARM64 names and units (`L1 data cache capacity`, KiB, capacity source);
  the load table has the same ten columns on every architecture (x86-64
  gains `Capacity Source` and `Cycle Source`, in KiB); the compute table's last
  column is `Latency (cycles)` everywhere. riscv64's load rows now come from
  the shared harness (same numbers expected within noise) and its
  `RVV compute:load` row sits in the issue table. Files moved: docs/,
  tools/, common/{cache,issue,sys}.
- **Under test:** `fix/review-high-medium` @ the commit after `370b534`
  (this checklist's own commit); the branch now contains main's Windows and
  RISC-V commits.
- **This round adds two things a GEMM kernel is designed around and cpufb
  did not measure:** the store path (store issue classes, L1/L2 store rows,
  memory write streams with a write-allocate verdict) and a prefetch probe
  (hardware prefetcher stride reach, stream restart cost, software prefetch
  distance and misses in flight). The latency curve in cycles from the last
  round is unchanged and still needs its `perf_event` check on the 920F.
- **Verified this round:** Neoverse V3, the Xeon VM and the Kunpeng 920F
  (job 1964626 on cn05122 from the `nsls_zhangxu` account, BiSheng clang,
  `ctest` 17/17 on all three; 920F numbers below with `perf_event cycles`).
  **Not verified:** Apple, Android, RISC-V (the prefetch probe is common
  code and runs there too; the store rows do not exist on riscv64).

## What changed this round

**Issue probe: store classes.** `str q`, `st1w z.s`, streaming `st1w` on
ARM64 and `vmovups [m], ymm/zmm` on x86-64, with the pairs `compute:store`,
`load:store` and `SME + streaming SVE store`. The kernel count grew from 330
to 546 on a full SVE+SME build, so `multi_issue` takes about 1.7x as long.
Expected: on a core with separate store pipes `load:store` reads
`independent`; `compute:store` tells whether stores take FSU slots.

**Load table: store rows** (`neon-st1h-4x1(f16) store`, `sve-st1h(f16)
store`, `movups/vmovups ... store`) at L1 and L2, same harness as the loads;
the columns `Load IPC` / `Bytes/Load` are now `Access IPC` / `Bytes/Access`.

**Cache table: memory write streams and `Write allocate`.** Full-line,
partial-line (16 B per 64 B line, counted as whole lines) and non-temporal
store streams over the memory workset; the verdict compares the first two
(a partial write must fetch the line; a full-line stream near 2x as fast did
not). V3 read 2.72x, `skip the line fill`; the Xeon VM 1.00x, `fetch the
line before writing it`, with `vmovntps` at 1.85x of the ordinary stores.

**Prefetch probe** (after the curve tables, four rows in the cache table):

| | V3 | Xeon VM |
|---|---|---|
| L2 workset: stride 1 hides | 61% of the L2 latency (1.68 ns vs 4.26) | 0% (no L2->L1 stride prefetch seen) |
| memory workset: stride 1 hides | 97% (2.46 ns vs 93 ns); strides up to 64 lines followed | 70% (32 ns vs 108 ns); reach 2 lines |
| stream restart cost | 131 ns = 53 line-times; even 1024-line segments +16% | 208 ns = 6.5 line-times |
| `prfm pldl1keep` / `prefetcht0`, memory | best 4.7 ns/line at distance 2 to 8, then worse | no effect beyond the hardware prefetcher |
| `prfm pldl2keep`, memory | 4.67 ns/line from distance 32 (20 lines in flight, 13.7 GB/s random) | no effect |

The V3 result that the L2-target prefetch sustains four times the random
stream the L1-target one does, and that `pldl1keep` gets worse with distance,
is a finding about the L1 miss queue; the 920F shows the opposite ranking
(`pldl1keep` 50 lines in flight, `pldl2keep` 22), so both are core
properties, not probe artefacts. Please check the V3 one repeats.

## Kunpeng 920F (PAC) — done this round, numbers for comparison

From the `nsls_zhangxu` account (`ssh sme`), inside
`/home/share/nsls_zhangxu/zl`, in a fresh directory that is removed after the
run (BiSheng clang from HPCKit 26.1.RC1, `TMPDIR` inside the tree, `dsub -s`
to `q_hpcapp`; the `zhounan` account can no longer submit). Results in
`benchmark_logs/Kunpeng920F/20261026_store_prefetch/`:

| | value | note |
|---|---|---|
| curve, cycle source | `perf_event cycles` | L1 4.0 cycles (2.585 ns), L2 11.9 cycles; last round's open item closed |
| L2 workset (156 KiB): stride 1 hides | 61% of 13.0 cycles; reach 8 lines | strides 16+ hide nothing |
| memory (64 MiB): random / stride 1 | 275 cycles / 5.7 cycles (98% hidden); reach 64 lines | |
| stream restart cost | 236 ns = 361 cycles = 63 line-times | even 1024-line segments +53% |
| `prfm pldl1keep`, memory | 4.07 ns/line at distance 64, 50 lines in flight, 15.7 GB/s | still falling at 64: the grid should go further here |
| `prfm pldl2keep`, memory | 9.21 ns/line from distance 16, 22 in flight | the opposite ranking to the V3 |
| write streams | full-line 45.7 GB/s, partial-line 11.2, `stnt1h` 45.1 (0.99x) | `skip the line fill`, 4.07x |
| issue: `str q` / `st1w` peak | 0.88 / 0.75 per cycle | tc3 measured `st1w` at 1.333 cycles each |
| `NEON load:store` | 2.00, shared ports | loads and stores share the two LSU slots |
| `SME + streaming SVE store` | 1.25 at 2:3, independent | FMOPA does not block streaming stores (it blocks streaming `fmla`) |

One oddity in the curve: 4 MiB read 309 ns against 88 ns at 8 MiB (a shared
last level under another job's load; the capacities were unaffected). Worth a
second run when the node is quiet.

**Still to do on the 920F:** extend the distance grid past 64 for the
`pldl1keep` column (it had not flattened), and a PMU-hidden run
(`perf_event_paranoid=3` is not settable there, so an unprivileged account
would be needed) to compare the clock-estimate cycles with the counted ones.

## Apple M4 Pro, Android, Milk-V X60

As in the last two rounds (M4 must build as committed; Android cache
fallback; X60 `L2 ways` repeatability), plus: the prefetch probe must print
its tables and the four rows must carry a verdict, never an empty cell
(`cpufb_cli_cache_probes` checks this). On the M4 the L2 workset is large
(16 MiB L2) and the probe takes longer; report the time.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| `pldl1keep` at memory worse with distance (V3) | Reported as measured; see the finding above. |
| Restart cost fit `C` while the points do not follow `T + C/S` exactly | The fit is a summary; the 10% line is read off the measured segments. |
| Cycle column moves with the clock estimate on hosts without a PMU | Expected; the header names the clock used. |
| Estimated clock under downclocking kernels (AVX-512 licences) | Charged as lost IPC; the Cycle source row says so. |
| One-point 97% band when two independent classes have equal peaks | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Graviton3 L3 capacity reads 8 to 16 MiB or `none` across runs | Sliced last level seen from one core. |
| X60 `L2 ways` read 12 / 9 / `hashed` across runs | Two passes must agree; a disagreement prints as not measured. |
| riscv64 has no issue probe and no store rows | Deliberate subset. |
| L3 ways and line; L2 ways on hashed L2 or without huge pages; M4 L1 line bound | Not measured or a bound, never guessed. |

## Reporting

State the commit tested. Send the `Prefetch probe` header and its three
tables, the cache table rows from `Memory sequential write bandwidth` to
`Software prefetch distance (L2 target)`, and the `multi_issue` rows that
contain `store`.
