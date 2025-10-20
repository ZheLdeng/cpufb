# Quick Reference

## One-Line Commands

```bash
# First-time setup
chmod +x build.sh

# Common commands
./build.sh              # Local build
./build.sh -a           # Android build
./build.sh -a --push    # Android build and push
./build.sh -r           # Clean rebuild
```

## Common Scenarios

| Scenario | Command |
|----------|---------|
| Local build | `./build.sh` |
| Android build | `./build.sh -a` |
| Push to device | `./build.sh -a --push` |
| Run on core 0 | `./build.sh -a --run-core 0` |
| Clean rebuild | `./build.sh -r` |
| Debug mode | `./build.sh -d` |
| Custom threads | `./build.sh -j16` |
| Show help | `./build.sh --help` |

## Migration from Old Scripts

| Old Command | New Command |
|-------------|-------------|
| `./build_x64.sh` | `./build.sh` |
| `./build_arm64.sh` | `./build.sh` |
| `./build_android.sh` | `./build.sh -a` |
| Manual adb push | `./build.sh -a --push` |

## All Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `-a, --android` | Build for Android |
| `-c, --clean` | Clean before building |
| `-j, --jobs N` | Parallel jobs count |
| `-d, --debug` | Debug mode |
| `-r, --rebuild` | Clean + build |
| `--push` | Push to device |
| `--run-core N` | Run on core N |
| `--toolchain FILE` | Custom toolchain |

## Troubleshooting

```bash
# Install cross-compiler
sudo apt-get install g++-aarch64-linux-gnu gcc-aarch64-linux-gnu

# Check Android connection
adb devices

# Force rebuild
./build.sh -r

# Verbose output
cd build && make VERBOSE=1
```

## Build Output

```
build/cpufb              # Native build
build-android/cpufb      # Android build
build/simd_config.cmake  # SIMD features
```

## Common Issues

| Issue | Solution |
|-------|----------|
| Cross-compiler not found | Install: `g++-aarch64-linux-gnu` |
| Device not found | Enable USB debugging |
| Permission denied | Run: `chmod +x build.sh` |
| Build fails | Try: `./build.sh -r` |

## Example Workflows

### Development
```bash
./build.sh              # Initial build
# edit code
./build.sh              # Rebuild
./build/cpufb           # Test
```

### Android Testing
```bash
./build.sh -a --push
adb shell /data/local/tmp/cpufb --thread_pool=[0]
```

### Multi-Platform
```bash
./build.sh              # Native
./build.sh -a           # Android
file build/cpufb build-android/cpufb
```