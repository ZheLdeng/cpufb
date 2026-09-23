# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `dcd07f2`
- **Already validated:** x86-64 and Kunpeng 920F, 14/14 each
- **Still to run:** MediaTek MT6993, Milk-V X60, and a confirmation pass on
  Kunpeng 920, Graviton3 and the M4 Pro

## Where the last round left things

Four platforms reported on `619b2f8`. Three of them closed everything they
had open, and the fourth produced the measurement this round is built on.

- **Kunpeng 920 (siat920)**: all six items passed. The memory reference is
  112-113 ns instead of 0.0, no row carries a prefetch warning, the L3 row
  reads 32 MiB and agrees with the OS, the table lists all four levels, ctest
  is 14/14, and the four capacities did not move.
- **Apple M4 Pro**: 18 of 19 runs read a 16 MiB L2, against 5 of 8 before,
  and three of those runs were under twelve competing processes. The
  `plateau drift` it now prints is 0.95-1.20, which is the quantity the
  previous round diagnosed and fixed.
- **Graviton3 (new platform)**: ctest 14/14, reference 113 ns, no warnings,
  L1/L2/ways/line all agree with sysfs. Its L3 reads half the OS value, which
  is not new and is probably not wrong; see the gaps table.
- **MediaTek MT6993**: the reference came back (0.0 to 212 ns), and the
  middle level stopped appearing and disappearing between runs. The control
  run settled what the previous two rounds could only suspect.

**Two things I wrote in the last checklist were wrong, and the reports caught
both.** The MT6993 reference reading 0.0 ns was put down to that device being
short of memory; it was the same `MemFree` query that affected a machine with
14.4 GB available. And I predicted the prefetch warning would disappear once
the reference came back: it did not, correctly, because the warning compares
the far end of the curve against the reference, and 26 ns against 212 ns is
exactly the case it exists for.

## What this round changes

Only one thing. **Page-grouped order is dropped when the memory reference
says it bought a curve that never left the prefetcher.**

It visits all 64 lines of a page in a row, which saves translation misses
where only 4 KiB pages are available, and is also the pattern a region
prefetcher serves best. On the MT6993 it served all of it:

| | page-grouped | globally shuffled |
|---|---|---|
| 128 MiB working set | 26.6 ns | 172.2 ns |
| 12 MiB step | falls | rises |
| prefetch warning | printed | absent |
| L1 | 64 KiB | 64 KiB |

Against a 212 ns reference the grouped sweep never got within an order of
magnitude of memory, so every capacity read from it was a prefetcher's. The
grouping stays for the misses it saves and is dropped when the reference says
otherwise. That test was not possible before this round, because the
reference read 0.0 ns on that device until the MemAvailable fix.

## MediaTek MT6993 (Android) — last tested at `619b2f8`

The platform this round is for. No environment variable is needed now.

```bash
for c in 0 1 7; do
  for i in 1 2 3; do
    CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>curve${c}_$i.txt
  done
done
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache
```

1. **Does the first line of each dump say `global order (page-grouped order
   never reached memory)`?** That is the fallback firing. If it still says
   `page-grouped order`, the reference or the comparison is wrong and that is
   the whole report.
2. **Is the far end of each curve now around 170 ns**, and does the latency
   rise everywhere? The dip appeared between 10 and 14 MiB in six runs of six
   before.
3. **What do cpu0 and cpu1 read now, three runs each?** Last round they were
   internally stable but differed from each other, 384 against 320 KiB, and
   cpu0's L3 gave three values. If the grouped order was the cause, both
   should settle; if they do not, that is the next thing to work on.
4. **What does cpu7 read?** It reported a 160 KiB L1 for a 64 KiB cache with
   the whole curve inside the prefetcher's reach. This is the cheap test of
   whether that was the measurement or the machine: if it returns 64 KiB, the
   conflict-stride probe in the gaps table is not needed.
5. **Does the prefetch warning disappear?** It should, on every core whose
   curve now reaches memory, and stay wherever one does not.

## Milk-V X60 (RISC-V) — last tested at `1f38cfb`, four rounds behind

Nothing here has been confirmed since the architecture gate was opened.

```bash
cmake --build build/native-release -j8 && cd build/native-release && ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
```

1. **ctest is 10/10**, where six CLI cases used to fail at an architecture
   gate inside `tests/test_cli.sh`.
2. **L1 32 KiB and L2 512 KiB, no dip, no warning.** This board has huge
   pages, so the change above should not touch it; it is the cheapest check
   that four rounds of work on the ring, the plateau rule, the level naming
   and now the page order broke nothing.

## Kunpeng 920, Graviton3, Apple M4 Pro — confirmation only

All three passed everything last round and none uses page-grouped order, so
one run each is enough to show this round did not disturb them.

```bash
ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve.txt
```

Kunpeng 920 should still read 64 KiB / 512 KiB / 4 MiB / 32 MiB with the
reference near 113 ns; Graviton3 64 KiB / 1 MiB / 16 MiB near 113 ns; the M4
128 KiB / 16 MiB with no warning. **On Graviton3, one extra thing worth a
line:** run `--thread_pool=[4]` once as well. If its L3 step is also at
16 MiB, that supports the reading in the gaps table; if it moves, the two
halves of that chip are not symmetric and the entry needs rewriting.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| Graviton3 reads a 16 MiB L3 where sysfs says 32 MiB | Stable across five runs and present two versions back, so not something this work introduced. The curve has a real 3.4x cliff between 16 and 20 MiB and none at 32, which is what a sliced last level looks like from one core: the OS reports the whole structure, the probe reports what this core can use. `DISAGREES with OS, 0.50x` states that accurately. |
| Kunpeng 920 resolves a step at 4 MiB inside its 32 MiB L3 | Same shape, one level down. Reported as a step below the L3 rather than suppressed. |
| MT6993 cpu7 reports a 128-160 KiB L1 for a 64 KiB cache | Under page-grouped order its whole curve stayed inside the prefetcher's reach. The change this round may fix it; if it does not, a conflict-stride sweep over the existing associativity ring is the candidate. |
| Capacities land on the sampling grid | By design: the answer is a working set that was actually measured. |
| Bandwidth worksets come from sysfs, capacities from the probe | They can disagree, as on a Kunpeng 920 and a Graviton3. Deliberate: the bandwidth path wants a workset that certainly fits, the capacity path must not be seeded from the OS. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Labelled when it is not sustained. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA, `.vs` forms | Only the accumulator path. The `.vv` forms have both since `88165b1`. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |

## Reporting

State the commit tested, and the Python version if ctest fails. Raw
`CPUFB_DEBUG_*` output is worth more than a summary of it, and a control run
is worth more than either: the change this round rests entirely on one pair
of sweeps that differed in one variable.
