# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ the commit after `7bf8930`
  (this checklist's own commit)
- **This round fixes what the last round found:** the Apple-assembler link
  failure, the ARM64 clock-source label, and three wrong expectations of the
  previous checklist. The issue probe's method and its kernels are unchanged;
  the arm64 and x86 generators now share one framework header.
- **Still to run:** Apple M4 Pro (the build must now link as committed),
  MediaTek MT6993 (never run), and the carried-over cache items.

## Where the last round left things

| | issue probe | notes |
|---|---|---|
| Kunpeng 920F (PAC) | 3/2/2/0.5, `Issue width` 4, FMOPA blocks streaming FMLA | run by me, `perf_event` cycles |
| Neoverse V3 (192 cores) | 6/4/3, `compute:load` 5 at 3:1, band 1:1 to 4:1 | matches `ISSUE_MODEL.md` |
| Graviton3 | 4/2/3, SVE `fmla` 1.84, `SVE compute:load` 3.61 at 1:1, width 7.9 | all seven references hit |
| Kunpeng 920 (siat920) | 3/2/2, width 4, `NEON compute:load` **2.92 at 2:1, partially shared** | see the corrected expectation below |
| Xeon Platinum VM | 4/2/2, AVX FMA 1.8, width 4 | estimated clock |
| Apple M4 Pro | **did not link** at `88699a0`; after a manual fix: ALU 7.9, FSU 3.23, LSU 3.0, `fmopa` 0.94, streaming `fmla` 0.23 | the fix is now in the tree |
| MT6993, Milk-V X60 | not run / not covered | |

## What changed this round

**The kernels link on Apple's assembler.** The ratio grid used to reach the
assembler as one line of `;`-separated macro calls; `;` is a statement
separator in GNU as and a comment in Apple's, which dropped 219 of 242
kernels. The grid is now handed over as one macro call with a variadic tail
(`ISSUE_PAIRS A, B, 1, 7, 1, 6, ...`) that the assembler unrolls one ratio per
recursion. GNU as verified here (330 kernels, counts and register partitions
audited from the disassembly); Apple's assembler needs the M4 run below.

**One framework, two architecture files.** `common/issue_asm.h` holds the
slot, partition, pattern and kernel-shape macros and the `.def` expansion;
`common/issue_kernel_tables.inc` holds the C++ tables; `<arch>/asm/_ISSUE_.S`
keeps only the instruction of each class, the register banks and the function
entry and exit, and `<arch>/kernel/issue_kernels.cpp` only the feature check.
Both drivers call `run_issue_probe_category()`.

**The `Cycle source` row on ARM64 names the frequency table's clock**
(`ADD-chain estimate`, `kperf fixed counters`, ...) instead of the generic
`frequency-table clock`, as x86 already did.

**Three expectations of the last checklist were wrong**, not the code:

- *Kunpeng 920 `NEON compute:load`.* I copied the 920F's 3.97 at 1:1. The
  920 measures 2.92 at 2:1, partially shared, three runs alike, while its
  `LSU + ALU` also stops at the 4-wide front end: a `ldr q` takes more than
  one issue slot there. The two cores differ; "equal peaks give a one-point
  band at 1:1" holds only when the classes are independent.
- *SME rows on a toolchain that rejects `+sme`* (GCC 12/13). The SME classes
  are not compiled, so there are no SME rows at all; the "not available"
  reason appears only for classes that were compiled and are absent at run
  time (SVE on the 920).
- *M4 `Cycle source` text*: see above.

## Apple M4 Pro (macOS) — must link as committed

```bash
cd ~/cpufb-m4-test && git checkout --detach <this commit>
cmake --preset native-release && cmake --build build/native-release -j8
cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --include-test=multi_issue --save=issue.csv
```

1. **The build links** without any edit; `nm` on `_ISSUE_.S.o` lists 242
   `issue_*` kernels (no SVE classes on this core).
2. **`Cycle source` reads `time x ADD-chain estimate (no cycle counter); ...`.**
3. **The table repeats the manual-fix run within 5%**: ALU about 7.9, FSU
   3.2, LSU 3.0, `fmopa` 0.94, streaming `fmla` 0.23, streaming `ld1w` 1.85,
   `NEON compute:load` about 6.3, `Issue width` about 9.8.
4. **Streaming `fmla` at 0.23 IPC** is the finding of the last run: on this
   core streaming-mode vector FMLA goes through a long-latency path, so
   `SME + streaming SVE compute` (1.17) is almost all FMOPA. If it repeats,
   it goes into the paper.

## MediaTek MT6993 (Android) — never run

Blocked on the udev rule for USB vendor `22d9` on siat920 (see the Android
report); the phone was not in `adb devices` on 2026-09-28. When it is back,
for cores 0, 1 and 7:

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

`L2 ways of associativity` flaps between 12, 9 and `hashed` (OS 16), which
fails `cpufb_cli_cache_probes` about half the time. Nothing changed there this
round; it is listed under Known gaps until the probe gets a two-pass agreement
rule. `L1 capacity from set conflicts` (32 KiB, 4 x 8 KiB) and both line rows
are correct and need no re-run.

## Graviton3, Kunpeng 920, Kunpeng 920F, V3 — nothing to run

Their issue tables were measured on the same kernels; this round changes only
how the kernels are emitted and one label. A quick `--include-test=multi_issue`
confirming the values did not move is welcome but not required.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| Estimated clock under downclocking kernels (AVX-512 licences) | Charged as lost IPC; the Cycle source row says so. |
| One-point 97% band when two independent classes have equal peaks | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Kunpeng 920 `NEON compute:load` 2.92 at 2:1 | A property of that core (loads cost more than one slot), not of the probe. |
| Three-class stream below a pair (V3: 8.0 against 9.8) | Loads take two dispatch slots; the width is taken from the pair and the row says so. |
| SME rows absent on GCC 12/13 builds | The toolchain rejects `+sme`; a row for classes that were not compiled may be added later. |
| Graviton3 L3 capacity reads 8 to 16 MiB or `none` across runs | Sliced last level seen from one core; the `re-measured` lines show 25 against 105 ns on the same working set. The L3 bandwidth row's workset follows it. |
| X60 `L2 ways` flaps 12 / 9 / `hashed` | Needs a two-pass agreement rule in `probe_l2_associativity`; not changed this round. |
| riscv64 has no issue probe | Deliberate subset. |
| L3 ways and line; L2 ways on hashed L2 or without huge pages; M4 L1 line bound | As before: not measured or a bound, never guessed. |

## Reporting

State the commit tested. Send the whole `multi_issue` table (or `issue.csv`)
and, for anything that is not the expected value, the Frontier and Verdict
text of that row verbatim. For the M4, `nm` output of `_ISSUE_.S.o` if the
link fails.
