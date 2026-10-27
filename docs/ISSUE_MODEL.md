# Compute/load/store issue model

`tools/issue_model.py` estimates the ideal per-component IPC of a fixed
instruction mix. It has profiles for the 192-core Neoverse V3 development
machine and both FP32 FMLA and BF16 BFMMLA on the 8-core Neoverse V1
machine. The V3 profile remains the default for compatibility.

```bash
./tools/issue_model.py 1F+2L
./tools/issue_model.py 4F+5L
./tools/issue_model.py 4F+3L+1S
./tools/issue_model.py 4F+3L+2S \
    --profile neoverse-v1-sve256-bfmmla
./tools/issue_model.py 4F+5L --json
```

`F` (or `C`) is a compute instruction, `L` is a load, and `S` is a store.
For the BFMMLA profile, `F` means one SVE `BFMMLA` instruction. Counts
describe one logical kernel group and may be fractional.

## Neoverse V3 default profile

For a group containing `F`, `L`, and `S` instructions:

```text
cycles/group = max(
    F / 4,
    L / 3,
    S / 2,
    (F + L) / 5,
    (F + S) / 4,
    (L + S) / 4,
    (F + L + S) / 10,
    (F + L + 2S) / 20
)
```

The limits are calibrated against CPU 96 on the 192-core Neoverse V3
machine. Rounded limits are used rather than fitting measurement noise:

| Mix | Predicted cycles/group | Measured cycles/group |
| --- | ---: | ---: |
| `1F+2L` | 0.6667 | 0.6682 |
| `2F+3L` | 1.0000 | 1.0096 |
| `3F+3L` | 1.2000 | 1.2126 |
| `4F+3L` | 1.4000 | 1.4171 |
| `4F+5L` | 1.8000 | 1.8128 |
| `2S` | 1.0000 | 1.0015 |
| `3L+1S` | 1.0000 | 1.0055 |
| `3L+2S` | 1.2500 | 1.2523 |
| `4F+1S` | 1.2500 | 1.2515 |
| `4F+3L+1S` | 1.4000 | 1.4022 |
| `4F+3L+2S` | 1.5000 | 1.5072 |

The store limit comes from the Arm Neoverse V3 Software Optimization Guide:
contiguous SVE stores have a documented throughput of two instructions per
cycle. `F+S` and `L+S` are derived from the documented V/V01 and L/SA
pipeline assignments. The store calibration rotates across 128 distinct
16-byte addresses in a 2 KiB L1-resident region. Loads and stores must be
finely interleaved: grouping three loads before every store was about 8%
slower for `3L+1S`, while an `L-S-L-L` order reached the ceiling above.

The dispatch limits model ten MOPs or twenty internal uOPs per cycle. A
contiguous SVE store contributes one architectural instruction but two
internal uOPs: store address and store data.

Official reference:
[Arm Neoverse V3 Core Software Optimization Guide](https://documentation-service.arm.com/static/6734eb2627eda361ad4da4f4).

## Neoverse V1 SVE256 BFMMLA profile

This is the 8-core BF16 matrix-compute profile:

```bash
./tools/issue_model.py 2F+2L \
    --profile neoverse-v1-sve256-bfmmla
```

Its measured piecewise ceiling is:

```text
cycles/group = max(
    F / 2,
    L / 2,
    S / 1,
    (F + L) / 3.67,
    (F + S) / 2,
    (L + S) / 2,
    (F + L + S) / 8,
    (F + L + 2S) / 16,
    F / 1.84             when L > 0,
    (F + S) / 1.86       when L > 0
)
```

The profile was calibrated on CPU 7 of `AmazonECS8Cores`, an eight-core
Neoverse V1 r1p1 system with 256-bit SVE. Every repetition executed 288
million independent logical groups. Loads were aligned L1 hits, and stores
rotated across 96 to 192 distinct 32-byte addresses in an L1-resident
region. The table uses the best PMU user-cycle count from five or six
repetitions.

| Mix | Predicted cycles/group | Measured cycles/group | Attainment |
| --- | ---: | ---: | ---: |
| `4F` | 2.0000 | 2.0032 | 99.8% |
| `2L` | 1.0000 | 1.0030 | 99.7% |
| `1F+1L` | 0.5450 | 0.5463 | 99.7% |
| `1F+2L` | 1.0000 | 1.0032 | 99.7% |
| `2F+1L` | 1.0870 | 1.0892 | 99.8% |
| `2F+2L` | 1.0899 | 1.0899 | 100.0% |
| `3F+3L` | 1.6349 | 1.6342 | 100.0% |
| `4F+4L` | 2.1798 | 2.1784 | 100.1% |
| `3F+4L` | 2.0000 | 2.1744 | 92.0% |
| `2S` | 2.0000 | 2.0032 | 99.8% |
| `4F+1S` | 2.5000 | 2.5038 | 99.8% |
| `4F+3L+1S` | 2.6882 | 2.7193 | 98.9% |
| `4F+3L+2S` | 3.2258 | 3.2262 | 100.0% |

Pure BFMMLA and pure load both sustain almost two instructions per cycle.
The balanced joint ceiling is 3.67 instructions/cycle: `2F+2L` sustains
about `1.835F+1.835L`. With load traffic present, the sustained BFMMLA
ceiling falls to about 1.84 instructions/cycle. BFMMLA and store share a
two-instruction ceiling without loads; with all three classes active,
`F+S` sustains about 1.86 instructions/cycle.

The source was also tested with four independent BF16 operand pairs, with
instruction-level interleaving, and with clustered compute/load blocks.
Those variants produced the same boundary. Eighteen of 19 selected
validation points are within 2%. `3F+4L` is the repeatable exception at
about 92% model attainment. Five-load groups were more variable and are
not used to fit the profile.

A 256-bit SVE BFMMLA performs 64 floating-point operations. The Arm guide
documents two BFMMLA instructions per cycle, an accumulate latency of three
cycles with forwarding, two contiguous SVE loads per cycle, and dispatch
limits of eight MOPs or sixteen uOPs per cycle.

Official reference:
[Arm Neoverse V1 Software Optimization Guide](https://documentation-service.arm.com/static/668ba8c29082ad344b14c3eb).

## Neoverse V1 SVE256 FMLA profile

The separately named FP32 FMLA profile is retained for workloads that use
ordinary vector FMLA rather than BF16 BFMMLA:

```bash
./tools/issue_model.py 4F+3L+2S \
    --profile neoverse-v1-sve256-fmla
```

Its upper-bound model is:

```text
cycles/group = max(
    F / 1.84,
    L / 2,
    S / 1,
    (F + L) / 3.6,
    (F + S) / 1.84,
    (L + S) / 2,
    (F + L + S) / 8,
    (F + L + 2S) / 16
)
```

The profile was measured on `AmazonECS8Cores`, an eight-core Neoverse V1
r1p1 system with 256-bit SVE. The calibration was pinned to CPU 7 and used
PMU user cycles. Each repetition executed 320 million logical groups; the
table uses the best of five to eight repetitions. Data was L1-resident,
instructions were independent, and stores rotated over 128 distinct
32-byte addresses.

| Mix | Predicted cycles/group | Measured cycles/group | Attainment |
| --- | ---: | ---: | ---: |
| `4F` | 2.1739 | 2.1753 | 99.9% |
| `3L` | 1.5000 | 1.5093 | 99.4% |
| `1F+2L` | 1.0000 | 1.0053 | 99.5% |
| `2F+3L` | 1.5000 | 1.6383 | 91.6% |
| `3F+3L` | 1.6667 | 1.6932 | 98.4% |
| `4F+4L` | 2.2222 | 2.2211 | 100.1% |
| `4F+5L` | 2.5000 | 2.7243 | 91.8% |
| `2S` | 2.0000 | 2.0029 | 99.9% |
| `3L+1S` | 2.0000 | 2.1804 | 91.7% |
| `3L+2S` | 2.5000 | 2.5036 | 99.9% |
| `4F+1S` | 2.7174 | 2.7176 | 100.0% |
| `4F+3L+1S` | 2.7174 | 2.7229 | 99.8% |
| `4F+3L+2S` | 3.2609 | 3.2632 | 99.9% |

The Arm guide gives peak throughputs of two SVE FMLAs, two contiguous SVE
loads, and two contiguous SVE stores per cycle. On this AWS instance, the
sustained full-operand FMLA test reached 1.84 instructions/cycle and the
256-bit store stream reached one instruction/cycle, so this machine profile
uses those measured effective ceilings. The measured shared limits are 3.6
for `F+L`, 1.84 for `F+S`, and 2 for `L+S`. The documented dispatch limits
are eight MOPs and sixteen uOPs per cycle.

Four of the 18 validation points are mix-sensitive: `2F+3L`, `4F+5L`,
`1S`, and `3L+1S` attain about 91% to 95% of the linear ceiling. Larger
unrolls, independent source registers, and alternate instruction orderings
did not remove those gaps. A single convex resource model cannot fit those
points without making faster neighboring mixes pessimistic, so the profile
keeps the useful upper bound and exposes the gap through
`--observed-cycles-per-group`. Fourteen of 18 measured points are within 2%,
and all are within 9%.

Official reference:
[Arm Neoverse V1 Software Optimization Guide](https://documentation-service.arm.com/static/668ba8c29082ad344b14c3eb).

## Metrics

- `compute/load/store IPC`: predicted instructions of that class per cycle.
- `compute efficiency`: compute IPC divided by the selected profile's peak.
- `load efficiency`: load IPC divided by the selected profile's peak.
- `store efficiency`: store IPC divided by the selected profile's peak.
- `memory efficiency`: the largest utilization among the load, store, and
  shared load+store constraints. This makes a pure three-load stream report
  100%, instead of incorrectly comparing it with an unrelated mixed maximum.
- `ops/cycle`: compute IPC times `--ops-per-compute` (eight for SVE128 and
  sixteen for SVE256 FP32 FMLA, or 64 for SVE256 BF16 BFMMLA).
- `read/write B/cycle`: load/store IPC times `--vector-bytes` (16 for the
  V3 profile and 32 for the V1 profile).

To evaluate an observed kernel rather than only the ideal ceiling:

```bash
./tools/issue_model.py 4F+5L --observed-cycles-per-group=1.812816
./tools/issue_model.py 4F+5L \
    --profile neoverse-v1-sve256-fmla \
    --observed-cycles-per-group=2.724299
./tools/issue_model.py 4F+3L+2S \
    --profile neoverse-v1-sve256-bfmmla \
    --observed-cycles-per-group=3.226180
```

To recalibrate a capacity without editing the script:

```bash
./tools/issue_model.py 4F+3L+2S \
    --set store=1.8 \
    --set compute_store=3.8 \
    --set load_store=3.6
```

## Scope

The model assumes:

- independent instructions and enough accumulator streams to hide latency;
- aligned L1 hits;
- enough unrolling to amortize loop control;
- no load-to-use, address-generation, or store-to-load dependency;
- no cache, TLB, DRAM-bandwidth, or branch-misprediction bottleneck.

It predicts an issue ceiling. Some calibrated profiles include empirical
constraints that activate only when load traffic is present. Real kernels
can only match or fall below the corresponding measured ceiling.
