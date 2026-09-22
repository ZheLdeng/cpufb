# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `af6c1a7`
- **Already validated:** x86-64 (14/14 ctest)
- **Still to run:** Apple M4 Pro, MediaTek MT6993, Milk-V X60

## What this round does, and why

The previous round asked for a capacity to sit within `1.25x` of the start of
its rise, on the reasoning that a real boundary makes the latency jump right
where the rise begins. Two reports measured that, and it is wrong in both
directions:

| | capacity / start of rise | what it should be | what the rule did |
|---|---|---|---|
| Apple M4 Pro L2 | 1.3 - 1.6 | 16 MiB, confirmed by sysctl | withheld, 16 of 17 runs |
| MT6993 cpu0 L2 | above 1.25 | 16 MiB, correct before | withheld |
| MT6993 cpu1 L2 | below 1.25 | 16 MiB | reported 384 KiB |
| MT6993 cpu7 L1 | 0.94 | withheld | reported 128 KiB |

The M4's L2 sits high because the plateau below it is not flat: it drifts
from 6.0 ns at 1 MiB to 17.4 ns at 14 MiB, so the 10% crossing of the step to
memory lands at 10 MiB, well under the 16 MiB boundary. The rule therefore
cost three correct capacities and never caught the core it was written for.
The width rule before it failed the same way from the other side.

**Both are gone.** Every level the curve resolves is reported. A rise wider
than one doubling is annotated, never vetoed. The M4 and MT6993 cpu7 curves
from the last two reports are now unit tests, so whatever replaces this has
to keep the first at 16 MiB and doubt the second.

The one thing the cpu7 curve does say is that it never reached memory: a
128 MiB working set still answers in 12.6 ns, 25 cycles at 2 GHz, which no
DRAM does. A chase that is prefetched at every size cannot place any
boundary, so that is now reported against every capacity from such a curve.

Everything else from the previous round stands: 32-bit chase links, the
riscv64 probes and CLI tests, `fmla.mul.vv`, the frequency and workset
warnings.

## Apple M4 Pro (macOS) — last tested at `fa2153d`

The machine that produced the clearest refutation, so the first to re-run.

```bash
ctest
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache --loop_scale=200
for i in $(seq 8); do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>&1 >/dev/null \
    | grep -E '^  (L[12]:|a )'
done
```

Please confirm:

1. **L2 reports 16 MiB in all eight runs**, not one in seventeen. The
   per-level line may say `wide` and the verdict may add `latency climbed
   over ... so the boundary is not sharp`; that is the annotation and it does
   not change the number.
2. **L1 still reports 128 KiB** and the line-size probe still reads 128 B.
   With L2 no longer withheld the eviction buffer returns to `4 x 16 MiB`
   rather than the 64 MiB floor, which is the case the probe was tuned for.
3. **No run prints the prefetch doubt.** This curve reaches 118 ns against a
   123 ns memory reference, so it must not be doubted.
4. **ctest is 14/14.**

## MediaTek MT6993 (Android) — last tested at `fa2153d`

```bash
for c in 0 1 4 6 7; do
  ./cpufb "--thread_pool=[$c]" --mode=cache --include-test=cache --loop_scale=200
done
for c in 0 1 7; do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>curve$c.txt
done
```

Please confirm:

1. **cpu0 and cpu1 report their L2 again**, around 14-16 MiB, instead of `-`
   and 384 KiB. This is the regression this round exists to undo.
2. **cpu7 reports a 128 KiB L1 with the doubt attached**: the verdict should
   carry `a 128 MiB working set still answered in 12.6 ns, so the chase was
   prefetched throughout and every capacity here may be too large`. The
   128 KiB is still wrong, the true L1 is 64 KiB, and the curve cannot see
   that; what it can say is that nothing here is trustworthy.
3. **cpu4 and cpu6 still report 64 KiB.**
4. `curve0.txt`, `curve1.txt` and `curve7.txt` in full. **The cpu0 and cpu1
   dumps are the most valuable thing in this round**: no curve from a working
   MT6993 L2 has ever been sent, and every rule proposed so far was designed
   without one. If only one thing comes back, make it those.

## Milk-V X60 (RISC-V) — last tested at `fa2153d`, which did not build

```bash
cmake --preset native-release && cmake --build build/native-release -j8
cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
CPUFB_DEBUG_CACHELINE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>line_x60.txt
```

Unchanged from the last checklist; none of it has run yet:

1. **It compiles.** `riscv_cache_chase32` is the fix for what the last report
   hit; paste any build error verbatim.
2. **ctest is 10, not 4.** Six CLI cases register on riscv64 now, and they run
   the `cpufb` binary, so ctest can no longer pass while the main program
   fails to build.
3. **The line size and the ways are measured**, not copied: the two rows
   should read `probe (agrees with OS)` rather than `Linux sysfs topology`,
   and still say 64 B and 4 ways. The probe makes its nodes cold by reading
   an eviction buffer of four times the measured L2, with no cache
   maintenance instruction, because `cbo.flush` needs Zicbom and the kernel's
   permission. If it reports `-`, send `line_x60.txt`.
4. **`curve_x60.txt` is no longer empty**; riscv64 did not call
   `debug_print_cache_curve` before.
5. **L1 32 KiB and L2 512 KiB, with no doubt attached** (this curve reaches
   220 ns, so it clearly gets to memory).
6. The line probe adds roughly 20-60 s to a cache run on this board. If it
   takes minutes, say so, and the eviction buffer can be bounded.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| MT6993 cpu7 reports a 128 KiB L1 for a 64 KiB cache | Its prefetcher follows the ring at every working set, so the curve genuinely shows L1 latency at 128 KiB. Two shape rules were tried and both broke correct levels elsewhere. The output now says the curve never reached memory. Placing this capacity needs a different measurement, not a better rule; a conflict-stride sweep over the existing associativity ring is the candidate. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. Capacity eviction alone is enough on the other two architectures. |
| L3 on a shared VM | Reads a few MiB against a 36 MiB nominal L3, and varies between runs. The measurable quantity there is the slice this guest gets. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Labelled when it is not sustained. |
| Bandwidth without sysfs cache sizes | Workset falls back to 256 MiB or is capped by free memory; not comparable across runs. Warned about. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA, `.vs` forms | Only the accumulator path. The `.vv` forms have both since `88165b1`. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |
| MT6993 memory reference reads 0.0 ns | The 1 GiB reference ring cannot be allocated under that device's memory pressure, which the workset-cap warning also reports. |

## Reporting

State the commit tested. Raw `CPUFB_DEBUG_*` output is worth more than a
summary of it: both rules removed this round were refuted by per-level dump
lines, and the dumps were right where the summaries were not.
