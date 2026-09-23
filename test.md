# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `8e8428a`
- **Already validated:** x86-64 (14/14 ctest) and Kunpeng 920F (14/14)
- **Still to run:** HiSilicon Kunpeng 920 (siat920), MediaTek MT6993,
  Apple M4 Pro, Milk-V X60

## What this round does, and why

All of it comes from the first siat920 report, which measured a bare-metal
Kunpeng 920 against its own sysfs and found two real bugs.

**The memory reference was never measured on that machine, and the reason was
an API choice, not the machine.** `sysconf(_SC_AVPHYS_PAGES)` answers with
MemFree, which on a host that has been up for a while is a small fraction of
what a process can get: 0.90 GB reported against 14.4 GB available. The
reference needs a 1 GiB region, so it was skipped, and with no reference the
curve could never be said to have reached memory, so the warning that the
chase had been prefetched throughout was printed against **every** capacity on
the machine, including rows that agreed with the OS exactly. It now asks for
MemAvailable.

**The L3 row took the third level rather than the deepest one.** That machine's
32 MiB L3 is sliced, so from one core the latency settles near 13.6 ns between
1.25 and 2.5 MiB, climbs, and settles again near 36 ns between 12 and 16 MiB
before memory. The curve resolves four levels and places the real boundary at
32 MiB exactly, but the row compared the OS against the third of them and read
`DISAGREES with OS, 0.12x`. The row is now the deepest level, and whatever the
curve found between L2 and it is named in the note. The estimate table also
stopped dropping everything past L2, which is why the four levels were only
visible in a debug dump.

**`math.lcm` needs Python 3.9** and Ubuntu 20.04 ships 3.8, so the pair-issue
unit test errored out there and ctest read 13/14 on an otherwise clean machine.

## HiSilicon Kunpeng 920 (siat920) — first report was `5b34077`

The most stable machine this project has: three cores, eight runs, capacities
and latencies identical to three decimals. That makes it the right place to
check a change to the capacity rules, where the M4's and the MT6993's own
variation would hide it.

```bash
cd build/native && ctest
for i in $(seq 3); do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>&1 >/dev/null \
    | grep -E '^cache curve|^  (L[0-9]:|a )'
done
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache
```

1. **`memory reference` is no longer 0.0 ns**, and **no row carries the
   prefetch warning any more**. The report measured 100.4-100.6 ns with the
   same change applied by hand, so this is a confirmation.
2. **The L3 row reads 32 MiB and agrees with the OS**, with a note saying the
   curve also stepped at 4 MiB below it. Previously it read 4 MiB and
   `DISAGREES with OS, 0.12x`.
3. **The estimate table now lists all four levels**, not just L1 and L2.
4. **ctest is 14/14**, not 13/14.
5. The four capacities themselves should not have moved: 64 KiB, 512 KiB,
   4 MiB and 32 MiB. If they did, that is the interesting part of the report.

## MediaTek MT6993 (Android) — last tested at `5b34077`

Two things, and the second is the one that matters.

```bash
for c in 0 1; do
  for i in 1 2 3; do
    CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache 2>&1 >/dev/null \
      | grep -E '^cache curve|^  (L[0-9]:|a )'
  done
done
CPUFB_CHASE_NO_PAGE_GROUPS=1 CPUFB_DEBUG_CACHE_CURVE=1 \
  ./cpufb '--thread_pool=[0]' --mode=cache 2>curve0_nogroup.txt
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve0.txt
```

1. **Does `memory reference` stop reading 0.0 ns here too?** It was put down
   to that device being short of memory, and the siat920 report shows that
   diagnosis was at least incomplete: a machine with 14.4 GB free of the
   kernel's reckoning had the same symptom from the same API. If the
   reference now reads something like 100 ns, the prefetch warning should
   disappear from cpu0 and cpu1 and stay on cpu7, whose curve tops out at
   12.6 ns and genuinely never reaches memory.
2. **`curve0_nogroup.txt` against `curve0.txt`.** Page-grouped order visits
   all 64 lines of a page in a row, which is what a region prefetcher serves;
   it is used only where pages are 4 KiB and huge pages are off. The switch
   shuffles globally instead. What matters is whether the 12 MiB dip survives
   and how high the far end of the curve gets.
3. **Do cpu0 and cpu1 agree with each other across three runs?** The plateau
   rule changed last round so that a 0.003 ns wobble can no longer decide
   whether a level exists.

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
   being cut in a different place, which is what the new plateau rule fixes. A
   stable 14 MiB is a pass; so is a stable 16.
2. **L1 128 KiB, line probe 128 B, ctest 14/14, no prefetch doubt.**

## Milk-V X60 (RISC-V) — last tested at `1f38cfb`, three rounds behind

```bash
cmake --build build/native-release -j8 && cd build/native-release && ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
```

1. **ctest is 10/10.** The six CLI cases failed at an architecture gate inside
   `tests/test_cli.sh` that still said x64 or arm64; `7d179b6` opened it. This
   is still the only fix this platform is waiting on.
2. **L1 32 KiB and L2 512 KiB, no dip, no doubt.** This board has the only
   textbook curve of the four, so it is the cheapest check that three rounds
   of changes to the ring, the plateau rule and the level naming broke
   nothing.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| A 0.9 ns dip at 12 MiB on MT6993, and a 128 MiB chase answering in 26 ns | Both point at page-grouped order. `CPUFB_CHASE_NO_PAGE_GROUPS=1` is the control; the measurement above decides what to change. |
| Kunpeng 920 resolves a step at 4 MiB inside its 32 MiB L3 | The curve really does flatten there, which is what a sliced last level looks like from one core. It is reported as a step below the L3 rather than suppressed, because no rule tried so far can tell an intermediate latency plateau from a capacity without inventing one. |
| MT6993 cpu7 reports a 128 KiB L1 for a 64 KiB cache | Its prefetcher follows the ring at every working set. Two shape rules were tried and both broke correct levels elsewhere. Placing this capacity needs a different measurement; a conflict-stride sweep over the existing associativity ring is the candidate. |
| Capacities land on the sampling grid | By design: the answer is a working set that was actually measured. |
| Bandwidth worksets come from sysfs, capacities from the probe | They can disagree, as they do on a Kunpeng 920 whose probe resolves an extra step. Deliberate: the bandwidth path wants a workset that certainly fits, the capacity path must not be seeded from the OS. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Labelled when it is not sustained. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA, `.vs` forms | Only the accumulator path. The `.vv` forms have both since `88165b1`. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |

## Reporting

State the commit tested, and the Python version if ctest fails. Raw
`CPUFB_DEBUG_*` output is worth more than a summary of it: the last three
rounds were each decided by two adjacent numbers in a dump.
