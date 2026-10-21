# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ the commit after `dd445c7`
  (this checklist's own commit)
- **This round:** the cache latency curve is reported in core cycles as
  well as nanoseconds. Issue kernels, cache probes and verdict texts are
  unchanged from `dd445c7`; everything listed there as still to run (M4 Pro
  build, Android fallback, X60 `L2 ways`, MT6993) is still to run and the
  instructions below still apply.
- **Verified this round:** Neoverse V3 and the Xeon VM (fallback clock path,
  `ctest` 16/16 on both). **Not verified:** the `perf_event` path, because
  the PAC login timed out; the Kunpeng 920F run below is the one that
  matters.

## What changed this round

**Every curve point and every level has a `Latency (cycles)` column.** The
seven chase samples of each point are each wrapped in a `perf_event` cycle
count; the fastest sample by time keeps its own count, so time and cycles
come from the same run. Without a counter the column is the latency times
the frequency table's clock, and the header line says which:

```
Cache curve translation mode: huge pages; cycles: perf_event cycles
Cache curve translation mode: huge pages; cycles: latency x 2.473 GHz (frequency table clock)
```

**`--mode=cache` now runs the frequency table first** so that the clock
exists on hosts without a PMU. The frequency table appears before the cache
tables; nothing else about the cache output changes.

**One printer for all three architectures.** The curve and level tables
come from `print_cache_curve_tables()` in `common/cache_curve.cpp`; the
ARM64, x86-64 and RISC-V front ends no longer carry their own copies, and
x86-64 prints them for the first time.

## Kunpeng 920F (PAC) — the `perf_event` path

Inside `/home/share/zhounan`, in a fresh directory that is removed after the
run:

```bash
source /work_ssd/software/HPCKit/26.1.RC1/setvars.sh
export TMPDIR=$PWD/tmpdir && mkdir -p tmpdir
cmake --preset native-release && cmake --build build/native-release -j16
cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --mode=cache | tee cache.txt
```

1. The header reads `cycles: perf_event cycles` and the column is filled.
2. **L1 reads 4 cycles** (`4.0` within 0.1; the 920F's load-to-use latency)
   and the ns column divided by the cycles column is the core clock in GHz
   on every row of the L1 plateau. The L2 plateau is expected near 12 and
   the LLC plateau around the value the `memory` row implies.
3. Compare the cycles with the clock-estimate numbers from a run with the
   PMU hidden (`perf_event_paranoid=3` or an unprivileged account): a
   difference above 5% on the L1 plateau is a finding about the estimate.

## Everything else — the ns column is untouched

A `ctest` run and one `--mode=cache` per machine is enough; the ns numbers
must match the last round within noise. Reference values from this round:

| | L1 | L2 | cycle source |
|---|---|---|---|
| Neoverse V3 (192 cores) | 64 KiB, 1.54 ns, 3.8 to 4.0 cycles | 1 MiB, 4.19 ns, 10.4 to 10.9 | 2.47 to 2.59 GHz estimate (run to run) |
| Xeon Platinum VM | 32 KiB, 1.61 ns, 4.5 | 1 MiB, 4.51 ns, 12.4 | 2.76 GHz estimate |

The V3's L1 reads 3.8 to 4.0 because the estimated clock moved between
runs; with a counter the column would not move. That is the point of the
Kunpeng check above.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| Cycle column moves with the clock estimate on hosts without a PMU | Expected; the header names the clock used. |
| Estimated clock under downclocking kernels (AVX-512 licences) | Charged as lost IPC; the Cycle source row says so. |
| One-point 97% band when two independent classes have equal peaks | Correct: T(x) = min(P/x, P/(1-x)) is a sharp peak. |
| Kunpeng 920 `NEON compute:load` 2.92 at 2:1 | A property of that core (loads cost more than one slot), not of the probe. |
| Three-class stream below a pair (V3: 8.0 against 9.8) | Loads take two dispatch slots; the width is taken from the pair and the row says so. |
| SME rows absent on GCC 12/13 builds | The toolchain rejects `+sme`. |
| Graviton3 L3 capacity reads 8 to 16 MiB or `none` across runs | Sliced last level seen from one core; the `re-measured` lines show it. |
| X60 `L2 ways` read 12 / 9 / `hashed` across runs | Two passes must agree; a disagreement prints as not measured. |
| riscv64 has no issue probe | Deliberate subset. |
| L3 ways and line; L2 ways on hashed L2 or without huge pages; M4 L1 line bound | Not measured or a bound, never guessed. |

## Reporting

State the commit tested. Send the `Cache curve` header line and both curve
tables (`--mode=cache` output), and for the 920F the same from a PMU-hidden
run next to it.
