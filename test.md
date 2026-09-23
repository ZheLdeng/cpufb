# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `c1b0eff`
- **Already validated:** x86-64 (14/14, every new row agrees with sysfs) and
  Kunpeng 920F (14/14, see below)
- **Still to run:** all five report platforms

## What this round does

The last checklist ended with a list of what cpufb still could not measure.
This round works through it.

**1. An L1 capacity that does not come from the latency curve.** Lines one way
apart share an L1 set, so twice the ways at that stride thrash one set, while
at half the stride they split over two and fit. The smallest stride that
conflicts is the way size, and ways x way size is the capacity. A prefetcher
can keep a pointer chase at L1 latency past the real capacity, which is what
the MT6993 big core does, but it cannot make lines share a set. The new row
is `L1 capacity from set conflicts`, and when the curve's L1 disagrees with it
by more than a grid step, the L1 row says the plateau is a prefetcher running
ahead of the chase.

**2. Line size from set indexing.** Of twice-the-ways lines in one set, every
other one is moved by d bytes. While d is inside a line they stay in the set;
at the line size they reach the next set and the conflict goes. This is a
cross-check on the reuse probe that no neighbour prefetch can widen. The row
is now `L1 cacheline size`, because the L1 line is all it ever measured.

**3. L2 ways and L2 line.** Lines 2 MiB apart in 2 MiB huge pages share every
physical bit below 21, so they meet in one L2 set too. Where that works the
rows are filled. Two cases leave them empty, and the row now says which: no
huge pages (4 KiB pages with THP off, or macOS), or huge pages present but no
L2 conflict at all, which means the L2 folds higher address bits into its set
index. The 920F is the second case.

**4. The samples that decide a result are measured three times.** The ones
either side of each capacity and either side of any fall get two more
measurements on fresh rings, and the median is used. This targets the M4's
L2 reading 12 or 14 in a fifth of its runs, and the MT6993 curve that fell by
a factor of three at one working set.

**5. The L3 stream stays inside what one core reaches.** On Graviton3 the OS
reports 32 MiB shared while one core's curve leaves cache at 16 MiB, so the
24 MiB "L3 bandwidth" stream was partly memory. When the curve measured a
smaller last level, the stream is capped at three quarters of it. The L3 row
also says, when the OS lists several CPUs sharing the level, that the OS
value is the whole cache and the probe what one core reaches.

**6. Smaller items.** Core Migration was always measured but printed before
it was known; it now prints at the end, and reads `not observed` when nothing
ran on the thread pool. `Theory Freq` is renamed `Reported Max Freq`. The
by-element FMA forms have a multiplier-path latency now, `fmla.mul.vs`.

## Kunpeng 920F (cluster) — already run at `b287f4f` and `c1b0eff`

| | sysfs | probe |
|---|---|---|
| L1 capacity from set conflicts | 32 KiB | 32 KiB, 8 ways x 4 KiB |
| L1 line, reuse and set indexing | 64 B | 64 B, both |
| L2 ways / line | 12 / 64 B | not measured: hashed index |
| FMA latency, accumulator / multiplier | | 2.38-2.50 / 4.38-4.50, `.vs` and `.vv` |

A diagnostic run showed the L2 region was backed by huge pages and that 32
lines sharing every bit below 21 stayed at L2 latency against 12 ways. So the
920F's L2 cannot be targeted by address, and the probe reports that rather
than a number.

## MediaTek MT6993 (Android) — last tested at `dcd07f2`

**The platform most of this round is for.**

```bash
for c in 0 1 7; do
  for i in 1 2 3; do
    CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache \
      >table${c}_$i.txt 2>curve${c}_$i.txt
  done
done
```

1. **What does `L1 capacity from set conflicts` read on cpu7?** Its real L1
   is 64 KiB with 4 ways, so the expected reading is `64 KiB, 4 ways x 16 KiB
   per way`. If it says that, the L1 row should carry `set conflicts put L1
   at 64 KiB ... so this plateau is a prefetcher staying ahead of the chase
   past it`. **This is the single most important line in the round.**
2. The same row on cpu0 and cpu1, which should also read 64 KiB and agree
   with the curve.
3. **Do cpu0 and cpu1 still fall between 6 and 14 MiB?** The decisive samples
   are now the median of three independent rings. If a fall survives that, it
   is not one lucky ring, and the working set it names is what to send.
4. **Is the middle level stable across three runs per core now?**
5. L2 ways and line will read `not measured: needs 2 MiB huge pages`, since
   Android has THP off. That is expected.

## Apple M4 Pro (macOS) — last tested at `dcd07f2`

```bash
ctest
for i in $(seq 8); do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>&1 >/dev/null \
    | grep -E '^  (L[12]:|latency fell|a )'
done
./cpufb '--thread_pool=[0]' --mode=cache | grep -E 'set conflicts|cacheline|ways of'
```

1. **The eight L2 readings.** Last round was seven of nine at 16 MiB. The
   decisive samples are now medians of three rings; whether that reaches
   eight of eight is the question.
2. **`L1 capacity from set conflicts` should read 128 KiB, 8 ways x 16 KiB**,
   and the line from set indexing 128 B. Apple's L1 has 16 KiB ways on 16 KiB
   pages, so the set index sits inside the page and should be targetable.
3. L2 ways will read `not measured: needs 2 MiB huge pages`. Expected.

## HiSilicon Kunpeng 920 (siat920) — last tested at `dcd07f2`

```bash
ctest
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>curve.txt >table.txt
```

1. `L1 capacity from set conflicts` should read 64 KiB, 4 ways x 16 KiB.
2. **L2 ways and line**: sysfs says 8 ways and 64 B. It has huge pages, so
   either the rows fill in and agree, or they say the index is hashed, as on
   the 920F. Either is a valid answer; a DISAGREES is not.
3. **The L2 cacheline row should add** `the OS reports 128 B lines at L3, which
   cannot be probed`. That is the case the old "cacheline size ... agrees"
   hid.
4. The L3 row should add that the OS value is shared by 8 CPUs.

## Graviton3 (AmazonECS8Cores) — last tested at `dcd07f2`

```bash
ctest
./cpufb '--thread_pool=[0]' --mode=cache
./cpufb '--thread_pool=[4]' --mode=cache
```

1. **The L3 bandwidth row's workset column** should now say `capped to 12 MiB,
   3/4 of the 16 MiB last level one core measured`. Last round it was 24 MiB,
   past what the core reaches. Compare the GB/s before and after: if it rose,
   the old row was partly measuring memory.
2. **The L3 capacity row** should add that the OS value is the whole cache
   shared by 8 CPUs.
3. `L1 capacity from set conflicts` should read 64 KiB, 4 ways x 16 KiB; L2
   ways and line either fill in (sysfs: 8 ways, 64 B) or say hashed.
4. The `[4]` run is the cross-check asked for last round: if its L3 also
   steps at 16 MiB, the shared-cache reading holds.

## Milk-V X60 (RISC-V) — last tested at `dcd07f2`

```bash
cmake --build build/native-release -j8 && cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --mode=cache
```

1. **ctest 10/10.** Last round's two failures were `--list-instructions`
   refusing to run without a thread pool; that is fixed.
2. `L1 capacity from set conflicts`: 32 KiB, 4 ways x 8 KiB. **This is the
   one platform where the L1 way (8 KiB) is larger than the page (4 KiB)**,
   so it tests whether its L1 is virtually indexed. A `no set conflict
   observed` answer would mean it is not, which is information, not a bug.
3. L2 ways and line: it has huge pages, sysfs says 8 ways / 64 B.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| L3 ways and L3 line | Not measurable by any method here: a last level shared by a chip is address-hashed over slices, and a line read from reuse timing cannot be told apart from a shorter line whose neighbour is fetched with it. The OS value is shown, labelled as such. |
| L2 ways and line on a hashed L2 (920F) or without huge pages (Android, macOS) | Reported as not measured, with the reason. |
| Why the MT6993 curve falls | Now either shown to be ring noise, if the median removes it, or confirmed as structural, if it does not. What answers there is still not established. |
| MT6993 has no OS cache values | Nothing there can be checked against the OS. The set-conflict L1 is the independent check that exists. |
| Kunpeng 920 resolves a step at 4 MiB inside its 32 MiB L3 | A sliced last level seen from one core. Reported as a step below the L3. |
| Capacities land on the sampling grid | By design: the answer is a working set that was actually measured. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. It reads the right size on the X60, and set indexing now cross-checks it. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |

## Reporting

State the commit tested. The new rows are the point of this round, so send
the whole cache table, not a summary. A `no set conflict observed` or a
`hashed` answer is a result; a DISAGREES is the thing to look at first.
