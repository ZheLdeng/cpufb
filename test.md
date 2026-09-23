# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `4f26ddd`
- **Already validated:** x86-64 and Kunpeng 920F, 14/14 each
- **Still to run:** Milk-V X60, MediaTek MT6993, and a one-run confirmation
  on Kunpeng 920, Graviton3 and the M4 Pro

## Where the last round left things

Five platforms reported on `dcd07f2`, and for the first time all five were on
the same commit.

- **Kunpeng 920, Graviton3, Apple M4 Pro**: confirmed, nothing new. The four
  Kunpeng capacities are unchanged to the digit, Graviton3 agrees with sysfs
  on everything except its L3, and the M4 read a 16 MiB L2 in seven runs of
  nine. None of them uses page-grouped order, so the change did not touch
  them, and the reports say so.
- **Milk-V X60**: the curve is still the cleanest of the five and every probe
  agrees with the OS. Its CLI cases ran against the binary for the first time
  and found a real defect, fixed this round.
- **MediaTek MT6993**: the fallback fired in nine runs of nine and lifted the
  far end from 26 ns to 146-195 ns, which is what it was for. It did not make
  the curve monotone, and one thing I wrote last round was wrong.

**The correction.** Last round's Android report said the globally shuffled
control rose throughout, and I repeated that here and in the commit that made
it the fallback. It did not: that control had a larger collapse at 7-8 MiB
than the 12 MiB dip it was being compared against, and the reading that it
"rises throughout" came from looking only at the points after 10 MiB. The
collapse is in both orders, in different places. **Switching the order did not
remove it and was never going to.**

## What this round changes

**A curve that falls is now doubted, not only one that stays shallow.** A
cache hierarchy cannot answer a larger working set faster, so a fall means
something outside the sweep is answering and the capacities are its. The old
test compared only the deepest point against the memory reference, and the
MT6993 big core shows what that misses: its far end reached 146 ns against a
156 ns reference, which cleared the test, while the middle of the curve
collapsed from 17.8 ns to 5.9 ns and its L1 read 256 KiB for a 64 KiB cache.
The warning cleared on a reading four times too large.

Only falls below half the memory latency count, and only falls of at least
10% and half a nanosecond. Against every curve on file that separates
cleanly: a Kunpeng 920F, a Kunpeng 920, an X60, an M4 Pro and the x86 VM fall
by at most 4.4% and never by as much as 0.2 ns, while the two MT6993 curves
fall by 11.6% and 25.3% at 12 MiB.

**And `--list-instructions` works on riscv64 without a thread pool**, as it
already did on the other two backends.

## Milk-V X60 (RISC-V) — last tested at `dcd07f2`

```bash
cmake --build build/native-release -j8 && cd build/native-release && ctest
./cpufb --list-instructions | head
```

1. **ctest is 10/10.** Two cases failed last round because
   `--list-instructions` exited 1 without `--thread_pool` on this backend
   alone; the listing now happens before that check. Nothing else on this
   platform needed work.
2. Nothing else. The curve, the capacities and the four probe verdicts were
   all confirmed last round and this round's changes do not touch them.

*One correction to the last report:* it listed `get_cachesize()` and
`get_multiway()` in `riscv64/kernel/load.cpp` as still-unhandled dead code.
They were deleted in `a6f53f8`, together with the `__APPLE__` branch; what
remains is the comment recording that.

## MediaTek MT6993 (Android) — last tested at `dcd07f2`

```bash
for c in 0 1 7; do
  for i in 1 2 3; do
    CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>curve${c}_$i.txt
  done
done
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache
```

1. **Does every core now carry a warning again, and does it name the fall?**
   The expected text is `latency fell from A ns to B ns between X and Y, which
   no cache does, so the capacities here are a prefetcher's`. On cpu7 that
   matters most: its far end clears the other test, so last round it printed
   no warning at all while reporting a 256 KiB L1.
2. **Which working set does it name, on each core and each run?** Last round
   the collapse was at 7-8 MiB in six runs of six under global order and at
   10-14 MiB under page-grouped order. Where it lands and how much it moves
   between runs is the most useful thing this round can bring back.
3. **Do the capacities themselves move?** They should not; only the warning
   changed. cpu0 read 64 KiB / 384 KiB, cpu1 64 KiB / 448-320-384 KiB, cpu7
   64 KiB of L1 read as 160-256 KiB.
4. If a run happens to produce a curve with **no** fall, its dump is worth
   more than the other three put together.

## Kunpeng 920, Graviton3, Apple M4 Pro — one run each

```bash
ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve.txt
```

None of them uses page-grouped order and none of them falls, so the only
question is whether the new warning stays silent. Kunpeng 920 should still
read 64 KiB / 512 KiB / 4 MiB / 32 MiB near 113 ns, Graviton3 64 KiB / 1 MiB
/ 14-16 MiB near 113 ns, the M4 128 KiB / 16 MiB. **A warning on any of these
is a false positive and the most important thing in the report.**

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| MT6993 curves fall somewhere in 6-14 MiB | Present under both page orders, in different places, so it is not the order. Now reported rather than suppressed, with the working set named. What is actually answering there is not established; a run whose curve does not fall would be the lead. |
| MT6993 cpu7 reads a 160-256 KiB L1 for a 64 KiB cache | Its prefetcher serves the chase at every working set under both orders, so this is not a matter of access order. A conflict-stride sweep over the existing associativity ring is the candidate, and it is the next substantial piece of work here. |
| Graviton3 reads a 14-16 MiB L3 where sysfs says 32 MiB | Stable across ten runs and present three versions back. The curve has a real cliff between 16 and 20 MiB and none at 32, which is what a sliced last level looks like from one core: the OS reports the whole structure, the probe reports what this core can use. |
| Kunpeng 920 resolves a step at 4 MiB inside its 32 MiB L3 | Same shape, one level down. Reported as a step below the L3 rather than suppressed. |
| M4 L2 varies by about 2 MiB around 16 | Seven runs of nine at 16 MiB, the rest at 12 or 14. Inherent to that machine, unchanged across the last three rounds. |
| Capacities land on the sampling grid | By design: the answer is a working set that was actually measured. |
| Bandwidth worksets come from sysfs, capacities from the probe | They can disagree, as on a Kunpeng 920 and a Graviton3. Deliberate. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Labelled when it is not sustained. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA, `.vs` forms | Only the accumulator path. The `.vv` forms have both since `88165b1`. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |

## Reporting

State the commit tested. Read the whole dump before concluding a curve is
monotone: the correction at the top of this file exists because a reading was
taken from five points out of sixty-one, and it cost a round.
