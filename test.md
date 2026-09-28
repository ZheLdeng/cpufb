# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `88699a0` (also merged to `main`)
- **This round:** the `multi_issue` category is a new measurement, the issue
  probe. It replaces the fixed `ldr/fmla` rows and needs one run per machine.
- **Already run:** Kunpeng 920F (PAC, SME), 192-core Neoverse V3, a Xeon
  Platinum VM. Results under `benchmark_logs/*/2026092*_issue_probe/`.
- **Still to run:** Graviton3, Kunpeng 920 (siat920), Apple M4 Pro, MediaTek
  MT6993, and the cache items carried over from the last round.

## What the issue probe measures

One command, one core, about 15 to 25 seconds:

```bash
./cpufb '--thread_pool=[N]' --include-test=multi_issue --save=issue.csv
```

The table has five columns: Item, Classes, IPC, Frontier, Verdict.

1. **Peak rows**, one per instruction class (`add x`, NEON `fmla`/`ldr q`,
   SVE `fmla`/`ld1w`, and on SME cores `fmopa` plus the streaming-mode
   `fmla`/`ld1w`; on x86 scalar `add`, SSE/AVX/AVX-512 FMA and loads). Each
   is a stream of independent instructions; the IPC is its peak. The
   Frontier column shows the half-chain and full-chain readings. `full
   chains needed` on an FMLA row is expected on 4-pipe cores (latency 4 x 4
   pipes needs 16 chains) and is information, not a fault.
2. **Pair rows**. For two classes the probe measures the total IPC of a mixed
   stream over 21 small-integer ratios plus the two pure streams, finds the
   maximum by golden-section search (about 7 runs) and the ratios within 3%
   of it by bisection. Frontier: `max at A:B = a:b; >= 97% from x to y (n of
   23 ratios run)`. Verdict: `independent: peaks add up`, `shared issue
   ports: mixing adds nothing`, `partially shared: k IPC ... shared budget`,
   or `shared and blocking: every mix issues slower than X alone`.
3. **Issue width**: the ALU+FSU+LSU stream nearest to the peak-proportional
   mix. The front-end width is the largest total IPC any row reached; pairs
   that reach it say `at the front-end issue width` or `bounded by the
   front-end issue width, not by shared ports`.
4. **Cycle source**: `perf_event cycles`, or `time x <clock>` when the PMU is
   denied. With an estimated clock, kernels that lower the core clock
   (AVX-512 licences) read low; that row says so.

## What to check on every machine

1. Every peak row has a value. A `-` means the kernel did not run.
2. Peaks against the published core: FMLA and load issue rates should match
   the core's documented pipe counts. Known: Kunpeng 920/920F 3/2/2
   (ALU/FSU/LSU), Neoverse V1 (Graviton3) about 6/2/2 with SVE `fmla`
   1.8-2, Neoverse V3 6/4/3, Apple M4 P-core 6+/4/3.
3. `SVE + NEON` reads `shared issue ports` on every Arm core tried; report
   anything else with the row.
4. `NEON compute:load` and `SVE compute:load`: the optimum and the 97% band.
   Equal FSU and LSU peaks (920F) give a one-point band at 1:1, which is
   correct; unequal peaks (V3: 3:1, band 1:1 to 4:1) give a range.
5. `Issue width` is at least the largest peak and at most the sum of the
   three. If a pair exceeds the three-class stream (V3: 9.8 against 8.0) the
   row says so; that is a finding about dispatch slots, not an error.
6. `ctest` passes, including the new `cpufb_issue_probe` unit test and the
   `cpufb_cli_issue_probe` CLI case.

## Per machine

**Graviton3 (AmazonECS8Cores, core 0 or 7).** Reference values are in
`ISSUE_MODEL.md`: SVE `fmla` about 1.84, loads 2, `SVE compute:load` joint
3.6-3.7 with the optimum near 1:1, dispatch 8. Expected `SVE + NEON` shared,
`FSU + ALU` independent.

**Kunpeng 920 (siat920, core 0).** Same core family as the 920F without
SME: expect ALU 3, FSU 2, LSU 2, `NEON compute:load` 3.97 at 1:1,
`Issue width` 4. The two SME rows must read `not available: sme_fmopa needs
_SME_`.

**Apple M4 Pro (macOS, a P-core).** Without root there are no counters;
the row reads `time x ADD-chain estimate`. Expect FSU 4, LSU 3, `SVE + NEON`
not applicable (no SVE: rows absent), and the three SME rows present: the M4
runs `fmopa` at about 1 per cycle, so `SME + streaming SVE compute` is the
interesting one. Run twice; report both if they differ by more than 5%.

**MediaTek MT6993 (Android, cores 0, 1 and 7).** Static cross build as
before. perf is root-only, so the clock is estimated; cpu7 runs at 2.0 GHz
against a 4.2 GHz cpufreq maximum, and the frequency table's `Test Freq`
must be the clock used. Expect the little cores at FSU 2, the big core at
FSU 4 with the `full chains needed` note.

**Milk-V X60 (RISC-V).** Not covered: the riscv64 backend keeps its own
`RVV_MULTI_ISSUE` row. Only the carried-over cache items below apply.

## Carried over from the cache round (unchanged code)

- **MT6993**: cpu7's `L1 capacity from set conflicts` should read `64 KiB,
  4 ways x 16 KiB per way`; cpu0/cpu1's fall between 6 and 14 MiB and the
  middle level's stability are still to be re-checked with the re-measured
  lines the debug dump now prints (`CPUFB_DEBUG_CACHE_CURVE=1`).
- **Milk-V X60**: `ctest` 10/10 (the `--list-instructions` gate is fixed),
  `L1 capacity from set conflicts` 32 KiB, 4 ways x 8 KiB, L2 ways/line
  filled in or `hashed`.
- **Apple M4 Pro**: the L1 line row should read `probe (agrees with OS);
  set indexing changes every 64 B, so the line is between 64 and 128 B ...`
  and not call the 128 B reading inflated.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| Estimated clock under AVX-512 or other downclocking kernels | Charged as lost IPC; the Cycle source row says so. Use counted cycles for those rows. |
| One-point 97% band when the two peaks are equal | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Three-class stream below a pair (V3) | Loads take two dispatch slots there; the row states the width from the pair. |
| riscv64 has no issue probe | Deliberate subset. |
| L3 ways and L3 line; L2 ways on a hashed L2 or without huge pages; L1 line bound on the M4 | As in the last round: reported as not measured or as a bound, never guessed. |

## Reporting

State the commit tested. Send the whole `multi_issue` table (or `issue.csv`)
and, for anything that is not the expected value, the Frontier and Verdict
text of that row verbatim.
