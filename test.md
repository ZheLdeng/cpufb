# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `a33f896`
- **Already validated:** x86-64 (13/13 ctest, L1/L2/L3 all agree with the OS)
  and Kunpeng 920F (three cluster jobs, 13/13 each, twelve cache runs, every
  one 32 KiB / 768 KiB / no L3 / 64 B)
- **Still to run:** MediaTek MT6993, Apple M4 Pro, Milk-V X60

## What changed since the last round (`88b3238`)

1. A capacity is no longer judged by how wide its rise is, but by where the
   capacity sits within that rise. A boundary makes the latency jump, so the
   capacity threshold is crossed where the rise starts; a prefetcher letting
   go turns the step into a slope and the threshold is crossed partway up it.
   Above `1.25x` of the start of the rise the capacity is withheld. This is
   what decides whether a level is reported at all, so every capacity on
   every platform is affected.
2. The chase links are 32-bit indices (`load_ptr32` on arm64, an int32 chase
   on x64), so a 64-byte line holds 16 of them and the ring is built from 16
   permutations instead of 8. This changes the shape of every measured curve.
3. `CPUFB_DEBUG_CACHE_CURVE` moved into `common/cache_curve` and now prints
   the per-level estimates as well as the sampled points. arm64 did not have
   this switch at all before.
4. A measured clock below 85% of the reported maximum adds `reported maximum
   not sustained under load` to the frequency source column.
5. A memory workset capped by available memory now also warns on stderr.

## MediaTek MT6993 (Android) — last tested at `88b3238`

This round exists because of this machine; it is the only one that can
confirm the fix.

```bash
./cpufb '--thread_pool=[7]' --mode=cache --include-test=cache --loop_scale=200
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[7]' --mode=cache 2>curve7.txt
./cpufb '--thread_pool=[0]' --include-test=freq
./cpufb '--thread_pool=[7]' --include-test=load
```

Please confirm:

1. **cpu7 no longer reports a 192 KiB L1.** The capacity cell should be `-`
   and the verdict should read `probe: L1 latency rises gradually from
   ~139 KiB to ~605 KiB (4.4x) instead of stepping, so no capacity is read
   off it; prefetcher suspected`. The exact numbers will differ; what matters
   is that a range is given and no capacity is claimed.
2. **cpu4 and cpu6 still report 64 KiB**, and cpu0/cpu1 still report
   64 KiB / 16 MiB. The new criterion must not have withdrawn a level that
   was correct at `88b3238`.
3. **The frequency table** says `reported maximum not sustained under load`
   on cpu4 and cpu7, and does not say it on cpu0.
4. **The bandwidth run** prints the workset-cap warning on stderr when the
   cap triggers, and says nothing when it does not.
5. `curve7.txt` in full, whatever the outcome. The per-level lines at the end
   of it are new and are what a wrong capacity has to be diagnosed from.

## Apple M4 Pro (macOS) — last tested at `86413b1`

Two rounds behind. Nothing here was written for this machine, but items 1 and
2 above change every capacity it measures, and its cluster-shared L2 is the
real-hardware case closest to the new threshold (it sits at 0.95 where the
cutoff is 1.25).

```bash
ctest
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache --loop_scale=200
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_m4.txt
CPUFB_DEBUG_CACHELINE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>line_m4.txt
```

Please confirm:

1. **L1 128 KiB and L2 16 MiB are still reported**, and neither is withheld.
   If the L2 is withheld, the per-level line in `curve_m4.txt` gives its rise
   and that is the number to report.
2. **The line-size probe still reads 128 B.** It runs after the curve and
   sizes its eviction buffer from the largest level the curve found, so a
   changed curve can change it.
3. **ctest is still 13/13** on Darwin.

## Milk-V X60 (RISC-V) — last tested at `88b3238`

The cheapest re-run of the three, and the only fixed-frequency machine with
complete sysfs cache data, so it is the cleanest check that the new ring did
not distort a textbook curve.

```bash
ctest
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
```

Please confirm L1 32 KiB and L2 512 KiB are still reported and neither is
withheld, and send `curve_x60.txt`.

Note that this backend exercises only `measure_cache_curve`. It calls neither
`probe_cacheline_size` nor `probe_l1_associativity`, and `tests/test_cli.sh`
is registered only for x64 and arm64, so four of the thirteen tests run here.
A green RISC-V run does not vouch for the probe paths.

## Known gaps — please do not re-report these

These are understood and either accepted or scheduled; a report that lists
them again costs a round trip.

| Gap | Status |
|---|---|
| MT6993 big core's true L1/L2 capacity | Unknown. Deliberately withheld rather than guessed. An independent probe could not resolve it either. |
| riscv64 line size and associativity | Copied from sysfs, labelled `Linux sysfs topology`, not measured. Fix is scheduled. |
| `test_cli.sh` not registered for riscv64 | Known. Fix is scheduled. |
| `get_cachesize()` / `get_multiway()` in riscv64 | Dead code, reads `index0` (instruction cache on X60), contains an `__APPLE__` block. Deletion is scheduled. |
| L3 on a shared VM | Reads a few MiB against a 36 MiB nominal L3, and varies between runs. The measurable quantity there is the slice this guest gets, not the hardware's L3. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Now labelled when it is not sustained, but the nominal number is still what the column shows. |
| Bandwidth without sysfs cache sizes | Workset falls back to 256 MiB or is capped by free memory; numbers are not comparable across runs. Now warned about. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` kernels of FMA instructions | The chain runs accumulator to accumulator, which on most cores is the shorter of the instruction's two input latencies (920F: 2.5 cycles against 4.5 through the multiplier input). The figure is real but is not "the" latency of the instruction. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |

## Reporting

State the commit tested. Raw `CPUFB_DEBUG_*` output is worth more than a
summary of it: every defect fixed in the last three rounds was diagnosed from
one of those dumps, and twice the reporter's conclusion was wrong while the
dump was right.
