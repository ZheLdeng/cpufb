# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `ea613ee`
- **Already validated:** x86-64 (14/14 ctest, L1/L2/L3 all agree with the OS)
  and Kunpeng 920F
- **Still to run:** Milk-V X60, MediaTek MT6993, Apple M4 Pro

**The RISC-V build break is fixed.** At `a33f896` and `fa2153d` the riscv64
backend did not compile: the 32-bit chase links were added to arm64 and x64
only. A reviewer hit it, patched a scratch copy the same way `a6f53f8` does,
and confirmed the curve is unchanged there (32 KiB and 512 KiB, clean steps),
so the ring change is not the risk on that platform. What is still unrun is
everything else in `a6f53f8`: the two probes, the CLI tests and the curve
dump. There is no riscv64 toolchain on the machine the branch is written on,
so the Milk-V remains the first compile of that code.

## What changed since the last round (`88b3238`)

1. A capacity is no longer judged by how wide its rise is, but by where the
   capacity sits within that rise. A boundary makes the latency jump, so the
   capacity threshold is crossed where the rise starts; a prefetcher letting
   go turns the step into a slope and the threshold is crossed partway up it.
   Above `1.25x` of the start of the rise the capacity is withheld. This
   decides whether a level is reported at all, so it affects every platform.
2. The chase links are 32-bit indices (`load_ptr32` on arm64,
   `riscv_cache_chase32` on riscv64, an int32 chase on x64), so a 64-byte
   line holds 16 of them and the ring is built from 16 permutations instead
   of 8. This changes the shape of every measured curve.
3. riscv64 measures its line size and L1 associativity instead of copying
   them from sysfs, and its dead second cache-size estimator is gone.
4. The CLI assertion that those two probes report a value now runs on every
   backend, as the `cache_probes` case. riscv64 had no CLI test before.
5. `fmla.mul.vv(f32,f32,f32)` and its f64 counterpart measure an FMA's
   latency through a multiplier input; the existing rows measure it through
   the accumulator. Both are real and they differ (920F: 2.31 against 4.44
   for f32, at the same 23.1 GFLOPS throughput).
6. `CPUFB_DEBUG_CACHE_CURVE` moved into `common/cache_curve` and prints the
   per-level estimates as well as the sampled points. arm64 and riscv64 did
   not have this switch at all before.
7. A measured clock below 85% of the reported maximum adds `reported maximum
   not sustained under load` to the frequency source column.
8. A memory workset capped by available memory now also warns on stderr.

## Milk-V X60 (RISC-V) — last tested at `88b3238`

```bash
cmake --preset native-release && cmake --build build/native-release -j8
cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --mode=cache --include-test=cache
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve_x60.txt
CPUFB_DEBUG_CACHELINE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>line_x60.txt
```

Please confirm, in this order:

1. **It compiles.** `riscv_cache_chase32` is the fix; paste any build error
   verbatim.
2. **ctest is 10, not 4.** Six CLI cases now register on riscv64 (`list`,
   `invalid_filters`, `filters_txt`, `mode_all_override`, `csv_output`,
   `cache_probes`); the sweep and memory-bandwidth cases stay out because the
   backend rejects those options. This also closes the hole the last report
   named: those cases run the `cpufb` binary, so ctest can no longer pass
   while the main program fails to build.
3. **The line size and the ways are measured**, not copied. The two rows
   should read `probe (agrees with OS)` rather than `Linux sysfs topology`,
   and should still say 64 B and 4 ways. This probe has never run on RISC-V:
   it makes its nodes cold by reading an eviction buffer of four times the
   measured L2, without any cache-maintenance instruction, because cbo.flush
   needs Zicbom and the kernel's permission. If it reports `-` or a wrong
   size, `line_x60.txt` has the ratios it saw and is what to send.
4. **L1 32 KiB and L2 512 KiB are still reported** and neither is withheld.
   The patched scratch build already showed this, so it is a confirmation,
   not an open question.
5. **`curve_x60.txt` is no longer empty.** riscv64 did not call
   `debug_print_cache_curve`; `ea613ee` wires it in.
6. The line probe adds roughly 20-60 s to a cache run on this board. If it
   takes minutes, say so, and the eviction buffer can be bounded.

## MediaTek MT6993 (Android) — last tested at `88b3238`

The only machine that can confirm what item 1 was written for.

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
   off it; prefetcher suspected`. The numbers will differ; what matters is
   that a range is given and no capacity is claimed.
2. **cpu4 and cpu6 still report 64 KiB**, and cpu0/cpu1 still report
   64 KiB / 16 MiB. The new criterion must not withdraw a level that was
   correct at `88b3238`.
3. **The frequency table** says `reported maximum not sustained under load`
   on cpu4 and cpu7, and does not say it on cpu0.
4. **The bandwidth run** prints the workset-cap warning on stderr when the
   cap triggers, and nothing when it does not.
5. `curve7.txt` in full, whatever the outcome.

## Apple M4 Pro (macOS) — last tested at `86413b1`

Three rounds behind. Nothing here was written for this machine, but items 1
and 2 change every capacity it measures, and its cluster-shared L2 is the
real-hardware case closest to the new threshold: it sits at 0.95 where the
cutoff is 1.25.

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
2. **The line-size probe still reads 128 B.** It sizes its eviction buffer
   from the largest level the curve found, so a changed curve can change it.
3. **ctest is 14/14** on Darwin (`cache_probes` is new).
4. The two `fmla.vv(f32,f32,f32)` latency rows, accumulator and `.mul`,
   whatever they say. An M4's FMA may well forward both inputs at the same
   time, in which case the two numbers are equal and that is the answer.

## Known gaps — please do not re-report these

These are understood and either accepted or scheduled; a report that lists
them again costs a round trip.

| Gap | Status |
|---|---|
| MT6993 big core's true L1/L2 capacity | Unknown. Deliberately withheld rather than guessed. An independent probe could not resolve it either. |
| riscv64 line-size probe runs without a flush instruction | By design: cbo.flush needs Zicbom and kernel permission. Capacity eviction alone is enough on the other two architectures. |
| L3 on a shared VM | Reads a few MiB against a 36 MiB nominal L3, and varies between runs. The measurable quantity there is the slice this guest gets, not the hardware's L3. |
| `Theory Freq` on a DVFS machine | The cpufreq maximum, not an operating point. Now labelled when it is not sustained, but the nominal number is still what the column shows. |
| Bandwidth without sysfs cache sizes | Workset falls back to 256 MiB or is capped by free memory; numbers are not comparable across runs. Now warned about. |
| `Core Migration` | Never implemented on any platform. |
| `*_latency` of FMA instructions, `.vs` forms | Only the accumulator path is measured for the by-element forms. The `.vv` forms now have both. |
| Line-size probe on a core that prefetches both directions | Reports twice the line size by construction. No machine tried so far does this. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options explicitly. |
| riscv64 thermal zone differs between runs | The system table reads whichever zone the kernel exposes; the reading itself was right both times. |

## Reporting

State the commit tested. Raw `CPUFB_DEBUG_*` output is worth more than a
summary of it: every defect fixed in the last four rounds was diagnosed from
one of those dumps, and twice the reporter's conclusion was wrong while the
dump was right.
