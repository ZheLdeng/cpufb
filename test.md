# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ the commit after `15e6cbc`
  (this checklist's own commit)
- **This round fixes what the consolidated report found:** the Apple
  assembler's 20-level macro nesting cap (the grid is now consumed four
  ratios per recursion), the cache fallback that never fired without a
  memory reference (Android), and the flapping X60 `L2 ways` (the transition
  must now repeat). The issue kernels are byte-identical to the last round.
- **Still to run:** Apple M4 Pro (the build must assemble and link as
  committed, no `-mllvm` flag), the Android phones (cache fallback), Milk-V
  X60 (`cpufb_cli_cache_probes` stability), MediaTek MT6993 (never run).

## Where the last round left things

| | issue probe | notes |
|---|---|---|
| Kunpeng 920F (PAC) | 3/2/2/0.5, `Issue width` 4, FMOPA blocks streaming FMLA | `perf_event` cycles |
| Neoverse V3 (192 cores) | 6/4/3, `compute:load` 5 at 3:1, band 1:1 to 4:1 | matches `ISSUE_MODEL.md` |
| Graviton3 | 4/2/3, SVE `fmla` 1.84, `SVE compute:load` 3.61 at 1:1, width 7.8 | all seven references hit, twice |
| Kunpeng 920 (siat920) | 3/2/2, width 4, `NEON compute:load` 2.92 at 2:1 | stable across rounds |
| Xeon Platinum VM | 4/2/2, AVX FMA 1.8, width 4 | estimated clock |
| Apple M4 Pro | assembled only with `-mllvm -asm-macro-max-nesting-depth=64`: ALU 7.9, FSU 3.3, LSU 3.0, `fmopa` 0.96, streaming `fmla` 0.24, width 10.0 | 21-level recursion hit the 20-level cap |
| Snapdragon 888 (A78) | 4/2/3, `compute:load` 4.0 at 2:3, width 6.0 | new; cache fallback did not fire |
| Snapdragon 710 | big and little read alike | downclocked; estimated clock |
| MT6993, Milk-V X60 | absent / unreachable | |

## What changed this round

**The grid recursion is four ratios deep, not twenty-one.** Apple's assembler
stops at 20 nested macros. `ISSUE_PAIRS` and `ISSUE_TRIPLES` now take four
ratios per call and recurse on the rest, so the 21-ratio pair grid is six
levels and the whole expansion, inner macros included, about twelve. GNU as
produces byte-identical kernels to the last round.

**The cache sweep gets a memory reference on hosts with less free memory.**
The reference ring used 1 GiB or nothing; a phone with under 4 GiB available
got nothing, and without a reference the "page-grouped order never reached
memory" fallback could not fire (`memory reference 0.0 ns` on both
Snapdragons). The region now halves from 1 GiB down to 256 MiB until a
quarter of the available memory holds it, and when even that fails the
fallback uses a 40 ns absolute floor (no DRAM answers faster; the MT6993
and the Snapdragon 888 answered 128 MiB in 13 ns). One helper,
`never_reached_memory()`, serves the fallback and the prefetch warning, and a
curve without a reference that is not clearly prefetched now says so instead
of being called prefetched.

**`L2 ways` must repeat.** The huge-page L2 probe runs its transition search
twice on independent rings and reports `not measured: the L2 conflict
transition did not repeat on a second ring` when they differ, instead of one
of the X60's 12 / 9 readings for a 16-way cache.

**The `Cycle source` row on ARM64 names the frequency table's clock**, as
verified on the M4 (`time x ADD-chain estimate (kperf: ...)`).

## Apple M4 Pro (macOS) — must build as committed

```bash
cd ~/cpufb-m4-test && git checkout --detach <this commit>
cmake --preset native-release && cmake --build build/native-release -j8
cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --include-test=multi_issue --save=issue.csv
```

1. **The build assembles and links** with the preset alone, no
   `-mllvm -asm-macro-max-nesting-depth`; `nm` on `_ISSUE_.S.o` lists 242
   `issue_*` kernels.
2. **The table repeats the last run within 5%**: ALU 7.9, FSU 3.3, LSU 3.0,
   `fmopa` 0.96, streaming `fmla` 0.24, streaming `ld1w` 1.9, `Issue width`
   10.0. Streaming `fmla` at 0.24 has now been seen four times and goes into
   the paper as a finding about the M4's streaming-mode vector path.
3. `ctest` 16/16.

## Android — the two Snapdragons, then the MT6993

**Snapdragon 888 (Mi 11, cpu4) and 710 (MI 8 SE, cpu7)**, cache only:

```bash
CPUFB_DEBUG_CACHE_CURVE=1 /data/local/tmp/cpufb "--thread_pool=[$c]" --mode=cache >table$c.txt 2>curve$c.txt
```

1. The curve header now names a memory reference (`memory reference NNN ns`,
   not `0.0`), if a quarter of `MemAvailable` reaches 256 MiB.
2. If the page-grouped sweep still ends far below it, the header reads
   `global order (page-grouped order never reached memory)`; with no
   reference at all, the same fallback fires when the deepest point is under
   40 ns. Either way the `prefetched throughout` warning should be gone
   unless the global sweep is prefetched too, which is then a finding; a
   curve with no reference and a slow deepest point now reads `no memory
   reference could be measured` instead.

**MediaTek MT6993** is blocked on the udev rule for USB vendor `22d9` on
siat920 and was not in `adb devices` on 2026-09-28. When it is back, for
cores 0, 1 and 7:

```bash
/data/local/tmp/cpufb "--thread_pool=[$c]" --include-test=multi_issue --save=issue$c.csv
CPUFB_DEBUG_CACHE_CURVE=1 /data/local/tmp/cpufb "--thread_pool=[$c]" --mode=cache >table$c.txt 2>curve$c.txt
```

Issue probe: perf is root-only, so the clock is estimated and the frequency
table's `Test Freq` is the clock used (cpu7 runs at 2.0 GHz against a 4.2 GHz
maximum). Expect the little cores at FSU 2 and the big core at FSU 4 with the
`full chains needed` note. Cache: cpu7's `L1 capacity from set conflicts`
should read `64 KiB, 4 ways x 16 KiB per way`, and the `re-measured` lines
show whether the 6 to 14 MiB fall on cpu0/cpu1 is one ring or all three.

## Milk-V X60 (RISC-V) — cache probes only

```bash
cd ~/cpufb-rt-new && cmake --build build/native-release -j8 && cd build/native-release
for i in 1 2 3; do ctest -R cpufb_cli_cache_probes; done
./cpufb '--thread_pool=[0]' --mode=cache | grep -E 'L2 ways|L2 cacheline'
```

1. **`cpufb_cli_cache_probes` passes three times in a row.** Last round it
   failed about every second run on an `L2 ways` reading of 12 or 9 against
   the OS's 16.
2. **The `L2 ways` row** reads either `16, probe (agrees with OS)` or
   `not measured: the L2 conflict transition did not repeat on a second
   ring`. A repeated wrong number (12 twice) would be a real disagreement
   and goes in the report with `CPUFB_DEBUG_ASSOCIATIVITY=1` output.

## Graviton3, Kunpeng 920, Kunpeng 920F, V3 — nothing to run

The issue kernels are byte-identical to the last round and the cache changes
only add a reference where there was none. A `ctest` run is welcome but not
required.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| Estimated clock under downclocking kernels (AVX-512 licences) | Charged as lost IPC; the Cycle source row says so. |
| One-point 97% band when two independent classes have equal peaks | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Kunpeng 920 `NEON compute:load` 2.92 at 2:1 | A property of that core (loads cost more than one slot), not of the probe. |
| Three-class stream below a pair (V3: 8.0 against 9.8) | Loads take two dispatch slots; the width is taken from the pair and the row says so. |
| SME rows absent on GCC 12/13 builds | The toolchain rejects `+sme`; a row for classes that were not compiled may be added later. |
| Graviton3 L3 capacity reads 8 to 16 MiB or `none` across runs | Sliced last level seen from one core; the `re-measured` lines show 25 against 105 ns on the same working set. The L3 bandwidth row's workset follows it. |
| X60 `L2 ways` read 12 / 9 / `hashed` across runs | Two passes must now agree; a disagreement prints as not measured. |
| riscv64 has no issue probe | Deliberate subset. |
| L3 ways and line; L2 ways on hashed L2 or without huge pages; M4 L1 line bound | As before: not measured or a bound, never guessed. |

## Reporting

State the commit tested. Send the whole `multi_issue` table (or `issue.csv`)
and, for anything that is not the expected value, the Frontier and Verdict
text of that row verbatim. For the M4, `nm` output of `_ISSUE_.S.o` if the
link fails.
