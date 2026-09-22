# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `24847ec` plus the switch below
- **Already validated:** x86-64 (14/14 ctest) and Kunpeng 920F (14/14, all
  five cache rows agree with the OS)
- **Still to run:** MediaTek MT6993, Apple M4 Pro, Milk-V X60

## What this round does, and why

The last round's two reports found the same root cause on two machines, and
this round fixes it.

A plateau used to be a run of samples staying within 1.10 of its own minimum,
so where one ended depended on where it began and on how much a slow drift
had accumulated. An MT6993 little core had a run measuring **1.099 against
the 1.10 limit**: a 0.003 ns wobble decided whether the estimator found two
cache levels or three, and the two cores of one cluster disagreed with each
other. On an M4 Pro the L2 climbs across its own range, and where that climb
was cut moved the plateau the capacity is measured from, which is what was
left of the 8-to-20 MiB spread after the ring fix.

A plateau now ends where the latency starts climbing, measured between
neighbouring samples as log latency over log working set. Within a level that
is 0.05 to 0.2 on every curve on file; a boundary is 1.4 to 10. The cut no
longer depends on the run so far.

This was validated offline against every curve this project has, old and new:
the 920F, the X60, this x86 VM and the M4 all keep the capacities they
already agreed with their OS on, exactly. The two MT6993 little cores go from
disagreeing with each other to reporting the same three levels, on the curves
measured before the ring fix and on the ones measured after. That pair is now
a unit test.

**Not fixed, and deliberately:** the 12 MiB dip the Android report found. See
below, it needs one measurement first.

## MediaTek MT6993 (Android) — last tested at `5b34077`

```bash
for c in 0 1; do
  for i in 1 2 3; do
    CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>&1 >/dev/null \
      | grep -E '^  (L[0-9]:|a )'
  done
done
# the new control: same core, no page grouping
CPUFB_CHASE_NO_PAGE_GROUPS=1 CPUFB_DEBUG_CACHE_CURVE=1 \
  ./cpufb '--thread_pool=[0]' --mode=cache 2>curve0_nogroup.txt
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve0.txt
```

1. **Do cpu0 and cpu1 now agree with each other, and with themselves across
   three runs?** Last round cpu0 gave three levels in three runs of five and
   cpu1 in two of three, from near-identical curves. Whether the middle level
   is real is a separate question; whether the two cores answer the same way
   is this round's.
2. **`curve0_nogroup.txt` against `curve0.txt`, the most useful thing this
   round.** The 12 MiB dip and the fact that a 128 MiB chase answers in 26 ns
   both point at page-grouped order: it visits all 64 lines of a page in a
   row, which is exactly what a region prefetcher serves. An M4 rises
   monotonically through the same working sets on huge pages. The new switch
   shuffles globally instead, paying a translation miss per access. What
   matters is **whether the dip survives** and **how high the far end of the
   curve gets**; if the ungrouped sweep reaches something like DRAM latency
   and climbs monotonically, page grouping is the thing to change next.
3. **cpu4, cpu6 and cpu7 unchanged**: 64 KiB, 64 KiB, 128 KiB with the doubt.

## Apple M4 Pro (macOS) — last tested at `5b34077`

```bash
ctest
for i in $(seq 8); do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>&1 >/dev/null \
    | grep -E '^  (L[12]:|a )'
done
```

1. **How far apart the eight L2 readings are.** Last round was 16, 14, 16, 12,
   8, 16, 16, 16, and the three outliers came from the plateau below the step
   being cut in a different place. That is what this round changes. A stable
   14 MiB is a pass; so is a stable 16.
2. **The `plateau drift` figure**, which was 1.18x last round against 1.56x
   before the ring fix. It should not have moved; it is printed to tell a
   drifting level from a flat one, not to be optimised.
3. **L1 128 KiB, line probe 128 B, ctest 14/14, no prefetch doubt.**

## Milk-V X60 (RISC-V) — last tested at `1f38cfb`, two rounds behind

Not re-run last round, so the one thing it reported is still unconfirmed:

```bash
cmake --build build/native-release -j8 && cd build/native-release
ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
```

1. **ctest is 10/10.** The six CLI cases used to fail at an architecture gate
   inside `tests/test_cli.sh` that still said x64 or arm64; `7d179b6` opened
   it. This is the only fix this platform is waiting on.
2. **L1 32 KiB and L2 512 KiB still, no dip, no doubt.** This board has the
   only textbook curve of the three, so it is the cheapest check that two
   rounds of changes to the ring and the plateau rule broke nothing.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| A 0.9 ns dip at 12 MiB on MT6993, and a 128 MiB chase answering in 26 ns | Both point at page-grouped order, which is used only where pages are 4 KiB and huge pages are off. `CPUFB_CHASE_NO_PAGE_GROUPS=1` is the control; the measurement above decides what to change. |
| MT6993 cpu7 reports a 128 KiB L1 for a 64 KiB cache | Its prefetcher follows the ring at every working set, so the curve genuinely shows L1 latency at 128 KiB. Two shape rules were tried and both broke correct levels elsewhere. The output says the curve never reached memory. Placing this capacity needs a different measurement; a conflict-stride sweep over the existing associativity ring is the candidate. |
| Capacities land on the sampling grid | By design: the answer is a working set that was actually measured. What was unstable was which plateau the threshold was measured from, and that is what this round changes. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. Capacity eviction alone is enough on the other two architectures. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Labelled when it is not sustained. |
| Bandwidth without sysfs cache sizes | Workset falls back to 256 MiB or is capped by free memory; not comparable across runs. Warned about. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA, `.vs` forms | Only the accumulator path. The `.vv` forms have both since `88165b1`. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |
| MT6993 memory reference reads 0.0 ns | The 1 GiB reference ring cannot be allocated under that device's memory pressure. The prefetch doubt covers the same ground more directly. |

## Reporting

State the commit tested. Raw `CPUFB_DEBUG_*` output is worth more than a
summary of it: the last two rounds were both decided by two adjacent numbers
in a dump, and neither would have shown in a table.
