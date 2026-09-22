# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `5b34077` plus the debug
  output below
- **Already validated:** x86-64 (14/14 ctest, L1 32 KiB and L2 1.0 MiB)
- **Still to run:** MediaTek MT6993, Apple M4 Pro, Milk-V X60

## What this round does, and why

**The ring had a step in it that no cache put there.** The Android report
found an MT6993 core answering 3.5 MiB in 6.7 ns and 4 MiB in 4.5 ns: a
larger working set served faster. The cause was the number of permutations
the ring is built from, which fell as the working set grew, and 4 MiB is
where 64-byte lines made it fall from two to one. One permutation is a single
fixed cycle, which that core's prefetcher follows, so the latency dropped
where the method changed. The estimator then read the step as a level: both
cores that showed it grew a 224-384 KiB level no other round has seen.

The count is now the same at every working set. What a sample is worth is no
longer tied to the ring length either, so the extra permutations cost
ring-build time and nothing else: a cache run on the x86 VM goes from 17 s to
36 s, and reads the same capacities.

**The riscv64 CLI cases were registered but gated off.** Six of them ran and
failed at the first line. The gate inside the script now includes riscv64.

**The capacity rule is unchanged, deliberately.** The M4's 8-to-20 MiB spread
and the MT6993's three-values-in-three-runs are both the same thing: the
capacity is the last sample below a latency threshold, and on those machines
the threshold falls between two samples. Taking the steepest interval instead
was tried this round and, on a machine whose L3 never forms a plateau,
returned that L3's boundary as the L2 capacity. A third rule invented against
the same two curves would be a third guess. **The curves this round makes it
possible to measure are what a better rule has to be chosen from**, which is
why the dumps below matter more than the table readings.

## MediaTek MT6993 (Android) — last tested at `1f38cfb`

The machine that found the ring bug, so the first to re-run.

```bash
for c in 0 1 4 6 7; do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>curve$c.txt
done
for c in 0 1; do
  for i in 1 2 3; do
    CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>&1 >/dev/null \
      | grep -E '^  (L[0-9]:|a )'
  done
done
```

Please confirm:

1. **The 4 MiB dip is gone** from `curve0.txt` and `curve1.txt`. The latency
   must not fall as the working set grows, anywhere in the sweep. If any dip
   remains, its working set is the single most useful number to report.
2. **What cpu0 and cpu1 read once the dip is gone.** The 224-384 KiB level
   may disappear, may stay, or may move. All three outcomes are informative
   and none is a failure; send the dumps rather than a verdict.
3. **How stable the capacities are across the three repeats per core.** The
   previous round gave cpu0 three values for one level in three runs. Whether
   that survives the ring fix decides whether the capacity rule still needs
   changing.
4. **cpu4, cpu6 and cpu7 are unchanged**: 64 KiB, 64 KiB, and 128 KiB with
   the prefetch doubt attached.

## Apple M4 Pro (macOS) — last tested at `1f38cfb`

```bash
ctest
for i in $(seq 8); do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>&1 >/dev/null \
    | grep -E '^  (L[12]:|a )'
done
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_m4.txt
```

Please confirm:

1. **How far apart the eight L2 readings are**, not whether they equal
   16 MiB. Last round they spanned 8 to 20 MiB over twenty samples, which is
   0.50x to 1.25x of the sysctl value; the question this round is the spread
   and the worst ratio, and the eight per-level lines answer it whatever it
   is. A stable 14 MiB would be a pass: it is 0.88x, inside the tolerance,
   and one answer rather than five.
2. **One full `curve_m4.txt`, and the `plateau drift` figure in it.** Last
   round's L2 plateau climbed from 6.0 ns at 1 MiB to 17.4 ns at 14 MiB,
   which is what pushes the 10% crossing below the capacity, and no output
   said so. The per-level lines now print it. Part of that drift was the ring
   itself: the permutation count fell from eight to one across exactly that
   range, so the number this round is the first honest measurement of it.
3. **L1 is still 128 KiB, the line probe still 128 B, ctest still 14/14, and
   no run prints the prefetch doubt.**
4. **What the line probe now says about its eviction buffer.** It prints
   `last_level=...KiB eviction=...MiB(4x level|clamped)`, so `4 x 16 MiB`
   and the 64 MiB floor are no longer the same output. This is the check
   that was undecidable on this machine last round.
5. A cache run takes about twice as long as before. If it becomes
   inconvenient, say so and the ring build can be bounded.

## Milk-V X60 (RISC-V) — last tested at `1f38cfb`

Everything the last report asked for is in, except the one it found:

```bash
cmake --build build/native-release -j8 && cd build/native-release
ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
```

1. **ctest is 10/10.** The six CLI cases failed at the architecture gate
   inside `tests/test_cli.sh`; the gate now includes riscv64. This is the
   only fix this platform needed.
2. **L1 32 KiB and L2 512 KiB still**, and the curve still has no dip. This
   board is the only one of the three with a textbook curve, so it is the
   cheapest check that the constant ring did not break anything.
3. The cache run was 34.7 s; expect roughly double.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| Capacities quantized to the sampling grid | Known, root-caused, deliberately not patched this round. The threshold rule puts the answer wherever the threshold falls between two samples. A replacement needs curves measured with the fixed ring. |
| MT6993 cpu7 reports a 128 KiB L1 for a 64 KiB cache | Its prefetcher follows the ring at every working set, so the curve genuinely shows L1 latency at 128 KiB. Two shape rules were tried and both broke correct levels elsewhere. The output says the curve never reached memory. Placing this capacity needs a different measurement, not a better rule; a conflict-stride sweep over the existing associativity ring is the candidate. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. Capacity eviction alone is enough on the other two architectures. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Labelled when it is not sustained. |
| Bandwidth without sysfs cache sizes | Workset falls back to 256 MiB or is capped by free memory; not comparable across runs. Warned about. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA, `.vs` forms | Only the accumulator path. The `.vv` forms have both since `88165b1`. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |
| MT6993 memory reference reads 0.0 ns | The 1 GiB reference ring cannot be allocated under that device's memory pressure. The prefetch doubt now covers the same ground more directly. |
| Nothing described the plateau a level sits on | Fixed: the per-level debug line prints `plateau drift`, the ratio across the plateau below the step. An Apple M4 Pro's L2 climbs 2-3x across its own range, which is why its capacity lands far above the start of its rise. |

## Reporting

State the commit tested. Raw `CPUFB_DEBUG_*` output is worth more than a
summary of it: the ring bug fixed this round was visible only as two adjacent
numbers in a dump, and no table reading would have shown it.
