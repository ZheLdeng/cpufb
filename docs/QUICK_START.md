# Quick Reference

## One-Line Commands

```bash
# First-time setup
chmod +x build.sh

# Common commands
./build.sh                    # native release build
./build.sh -d                 # native debug build
./build.sh -a                 # cross-compile for ARM64 (Android-friendly)
./build.sh -a --push          # ...and adb-push the binary
./build.sh -r                 # wipe build dir + rebuild
```

Output binary: `build/<preset>/cpufb`.

On Windows x64, install MinGW-w64 GCC, CMake, and make with the POSIX thread
model from an MSYS2 MINGW64 terminal:

```sh
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
```

Then run from PowerShell with MSYS2 MINGW64 on `PATH`:

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --preset windows-mingw
cmake --build --preset windows-mingw
build\windows-mingw\cpufb.exe '--thread_pool=[0]' --mode=compute
```

For complete Windows output, run all categories without a benchmark limit:

```powershell
build\windows-mingw\cpufb.exe '--thread_pool=[0]' --mode=all
```

For a shorter functional check with the same rows, add `--loop_scale=100`
(an integer loop-count divisor, not a fractional multiplier). Do not add
`--bench_limit=1` or ISA filters when comparing output coverage. Results
from shortened runs are not suitable for performance comparisons.
The memory-bandwidth stream test is a separate invocation:

```powershell
build\windows-mingw\cpufb.exe '--thread_pool=[0]' --memory-bandwidth --memory-size-mib=4 --memory-repetitions=1
```

Windows reports base/max frequency from CPUID and current frequency from
the power-information API when available. The current value is an OS
report, not a measured benchmark clock; unavailable values have an explicit
reason in the source column. Current-frequency reporting is unavailable
on systems with multiple processor groups. Timestamps use a numeric UTC
offset rather than a localized timezone name. Hardware cycle counters
remain unavailable on this Windows backend; cycle metrics are estimates.

Benchmark modes:

```bash
build/native-release/cpufb '--thread_pool=[0]' --mode=cache
build/native-release/cpufb '--thread_pool=[0]' --mode=compute
build/native-release/cpufb '--thread_pool=[0]' --mode=all
```

## Common Scenarios

| Scenario                    | Command                             |
| --------------------------- | ----------------------------------- |
| Local release build         | `./build.sh`                        |
| Local debug build           | `./build.sh -d`                     |
| Android cross-compile       | `./build.sh -a`                     |
| Push to device              | `./build.sh -a --push`              |
| Run on core 0               | `./build.sh -a --run-core 0`        |
| Clean rebuild               | `./build.sh -r`                     |
| Use a specific CMake preset | `./build.sh --preset macos-arm64`   |

## Direct CMake usage

```bash
cmake --list-presets
cmake --preset native-release
cmake --build --preset native-release
cmake --install build/native-release --prefix /tmp/cpufb-install
```

Available configure presets: `native-release`, `native-debug`, `macos-arm64`,
`aarch64-cross`, `riscv64-cross`.

## SIMD detection override

By default the configure step runs the per-arch `cpuid` detector via
`try_run`. When cross-compiling that's not possible; pass the feature list
explicitly:

```bash
cmake --preset aarch64-cross \
    -DCPUFB_SIMD_FEATURES_OVERRIDE="_BF16_;_I8MM_;_SVE_;_SVE2_"
```

## Legacy scripts (removed)

The per-architecture shell scripts no longer listed the shared sources and
could not link, so they were removed. Their replacements:

| Old command            | New command                                              |
| ---------------------- | -------------------------------------------------------- |
| `./build_x64.sh`       | `./build.sh` (on x64 host)                               |
| `./build_arm64.sh`     | `./build.sh` (on arm64 host) or `--preset macos-arm64`   |
| `./build_android.sh`   | `./build.sh -a` (or `cmake --preset aarch64-cross`)      |
| `./build_riscv64.sh`   | `./build.sh --preset riscv64-cross`                      |
| `./clean.sh`           | `rm -rf build/`                                          |

## Troubleshooting

```bash
# Force a clean reconfigure
./build.sh -r

# Verbose build output
cmake --build --preset native-release --verbose

# See which SIMD features were detected
grep "detected SIMD" build/native-release/CMakeFiles/CMakeOutput.log \
  || cmake --preset native-release  # re-run configure to print it
```

| Issue                      | Solution                                              |
| -------------------------- | ----------------------------------------------------- |
| `cmake --preset` missing   | Upgrade CMake to 3.19+                                |
| Cross-compiler not found   | Install `g++-aarch64-linux-gnu` / `riscv64-linux-gnu` |
| adb device not found       | Enable USB debugging; check `adb devices`             |
| `Permission denied`        | `chmod +x build.sh`                                   |
