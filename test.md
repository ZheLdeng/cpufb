# Re-test checklist

Rewritten in full at the end of every round. It describes what the branch
needs verified *now*; it is not a log, and nothing is appended to it.

- **Under test:** `fix/review-high-medium` @ `455f40e`
- **Already validated:** x86-64 (14/14)
- **Still to run:** MediaTek MT6993 and Milk-V X60, which both missed the
  last round; a short pass on the M4 Pro

## Where the last round left things

Three of five platforms reported on `c1b0eff`.

| | result |
|---|---|
| Graviton3 | **all seven checks passed.** The L3 stream, capped from 24 MiB to 12 MiB, rose from 56.0 to 58.0 GB/s, which confirms the old row was partly measuring memory. The shared-L3 note appeared, and core 4 steps at 14 MiB as core 0 does. |
| Kunpeng 920 (siat920) | **five of six.** L1 from set conflicts 64 KiB (4 x 16 KiB); L2 ways and line filled in and agree with sysfs (8 ways, 64 B); the L2 line row names the 128 B L3 line. The sixth was my mistake, below. |
| Apple M4 Pro | L1 from set conflicts 128 KiB (8 x 16 KiB), as predicted. The set-indexing line read 64 B against the OS's 128 B; see the first change below. L2 read 16 MiB seven times of eight and 20 MiB once. |
| MT6993, Milk-V X60 | not run; see their sections. |

**My mistake on siat920.** The last checklist expected its L3 row to say the
OS value is shared by 8 CPUs. That note only explains a probe reading *below*
the OS value, as on Graviton3 (16 against 32 MiB). On siat920 the probe reads
32 MiB, equal to the OS, so there is nothing to explain and the note is
correctly absent. The code was right and the expectation was wrong.

## What changed this round

**The set-index granule now bounds the line instead of overruling it.** The M4
has 128 B lines kept in 64 B indexed sectors, so moving an address by 64 B
changes its set without leaving the line. The set-indexing probe measures that
granule. It is a lower bound on the line, as the reuse probe is an upper one,
and a 64 B-sectored 128 B line looks exactly like a 64 B line whose neighbour
is always fetched with it. Last round the row picked the second explanation
and told the M4 its correct 128 B was inflated. It now states the bound and
both explanations, and picks neither.

**The debug dump shows the three rings behind every re-measured sample**, as
the macOS report asked, so an outlier that survives the median can be seen
to be two rings out of three rather than one.

## MediaTek MT6993 (Android) — last run at `dcd07f2`, two rounds behind

**Blocked last round, not by cpufb.** The phone enumerates with USB vendor
`22d9`, which siat920's udev rules do not cover, so `adb` cannot open it and
that account has no passwordless sudo. Someone with sudo on siat920 needs to
run:

```bash
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="22d9", MODE="0666", GROUP="plugdev"' \
  | sudo tee /etc/udev/rules.d/52-oneplus.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
# then unplug and replug the phone
```

Then, for cores 0, 1 and 7, three runs each:

```bash
CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb "--thread_pool=[$c]" --mode=cache \
  >table.txt 2>curve.txt
```

1. **cpu7's `L1 capacity from set conflicts`.** Its L1 is 64 KiB, 4 ways; the
   expected reading is `64 KiB, 4 ways x 16 KiB per way`, and the L1 row
   should then add that the curve's 160-256 KiB plateau is a prefetcher
   staying ahead of the chase. **This is still the most important line
   outstanding anywhere.**
2. **Do cpu0 and cpu1 still fall between 6 and 14 MiB?** The dump now lists
   the three rings behind every re-measured sample. If a fall survives, those
   lines show whether all three rings saw it.
3. **Is the middle level stable across three runs?**
4. L2 ways and line will read `needs 2 MiB huge pages`; Android has THP off.

## Milk-V X60 (RISC-V) — last run at `dcd07f2`, two rounds behind

```bash
cmake --build build/native-release -j8 && cd build/native-release && ctest
./cpufb '--thread_pool=[0]' --mode=cache
```

1. **ctest 10/10.** `--list-instructions` no longer needs a thread pool.
2. **`L1 capacity from set conflicts`: 32 KiB, 4 ways x 8 KiB.** This is the
   one platform whose L1 way (8 KiB) is larger than its page (4 KiB), so it
   tests whether that L1 is virtually indexed. `no set conflict observed`
   would mean it is not, which is information, not a bug.
3. **L2 ways and line**: it has huge pages and sysfs says 8 ways / 64 B.
   Filled in and agreeing, or `hashed`, are both valid answers.

## Apple M4 Pro (macOS) — short pass

```bash
./cpufb '--thread_pool=[0]' --mode=cache | grep cacheline
for i in $(seq 8); do
  CPUFB_DEBUG_CACHE_CURVE=1 ./cpufb '--thread_pool=[0]' --mode=cache 2>&1 >/dev/null \
    | grep -E '^  (L2:|re-measured)'
done
```

1. **The L1 line row should read** `probe (agrees with OS); set indexing
   changes every 64 B, so the line is between 64 and 128 B: either the cache
   indexes 64 B sectors of the line, or a neighbour is fetched with it`. It
   must no longer say the 128 B reading is inflated.
2. **If any of the eight L2 readings is not 16 MiB**, send its re-measured
   lines. Those show which working sets decided it and what each ring read.

## Kunpeng 920, Graviton3 — nothing to run

Both passed everything that applies to them last round, and this round
changes only a message they do not print (their two line probes agree) and a
debug line.

## Known gaps — please do not re-report these

| Gap | Status |
|---|---|
| L3 ways and L3 line | Not measurable by any method here. A last level shared by a chip is address-hashed over slices, and a line read from reuse timing cannot be told from a shorter line whose neighbour is fetched with it. The OS value is shown, labelled as such. |
| L2 ways and line on a hashed L2 (920F, Graviton3) or without huge pages (Android, macOS) | Reported as not measured, with which of the two reasons applies. |
| L1 line where the two probes differ (M4) | Reported as a bound, 64-128 B, with both explanations. Only the OS knows which. |
| Why the MT6993 curve falls | Unresolved; the device has not been reachable since the median was added. |
| MT6993 has no OS cache values | The set-conflict L1 is the independent check that exists. |
| Kunpeng 920 resolves a step at 4 MiB inside its 32 MiB L3 | A sliced last level seen from one core, reported as a step below the L3. |
| Graviton3's L3 bandwidth workset varies between 9 and 12 MiB | It is 3/4 of the L3 one core measured, which itself reads 12-16 MiB. Both give 57-58 GB/s. |
| Capacities land on the sampling grid | By design: the answer is a working set that was actually measured. |
| riscv64 line-size probe runs without a flush instruction | By design: `cbo.flush` needs Zicbom and kernel permission. |
| riscv64 has no memory-bandwidth, instruction sweep or loop scaling | A deliberate subset; the backend rejects those options. |

## Reporting

State the commit tested. Send the whole cache table and the re-measured lines
for anything that is not the expected value.
