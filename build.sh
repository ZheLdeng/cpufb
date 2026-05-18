#!/usr/bin/env bash
# Thin wrapper around `cmake --preset` for cpufb.
#
# Most users only need:
#   ./build.sh                      # native release build
#   ./build.sh -d                   # native debug build
#   ./build.sh -a                   # cross-compile to aarch64 (Android-friendly)
#   ./build.sh -a --push            # ...and adb-push to /data/local/tmp
#   ./build.sh -a --run-core 0      # ...and run pinned to core 0
#
# For anything outside of these shortcuts, just call cmake directly:
#   cmake --preset <name> && cmake --build --preset <name>

set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

  -h, --help              Show this help.
  -a, --android           Cross-compile to aarch64-linux-gnu (Android-friendly).
  -d, --debug             Use the native-debug preset.
  -r, --rebuild           Wipe build directory before configuring.
      --preset NAME       Use an explicit CMake preset (overrides -a/-d).
      --push              After build, adb-push the binary to /data/local/tmp.
      --run-core N        After --push, run pinned to core N via taskset.

Available CMake presets:
EOF
    cmake --list-presets 2>/dev/null | sed -n '/^[[:space:]]*"/p'
}

ANDROID=false
PRESET=""
DEBUG=false
REBUILD=false
PUSH=false
RUN_CORE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)     usage; exit 0 ;;
        -a|--android)  ANDROID=true; shift ;;
        -d|--debug)    DEBUG=true; shift ;;
        -r|--rebuild)  REBUILD=true; shift ;;
        --preset)      PRESET="$2"; shift 2 ;;
        --push)        PUSH=true; shift ;;
        --run-core)    RUN_CORE="$2"; shift 2 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ -z "$PRESET" ]]; then
    if   $ANDROID; then PRESET="aarch64-cross"
    elif $DEBUG;   then PRESET="native-debug"
    else                PRESET="native-release"
    fi
fi

BUILD_DIR="build/${PRESET}"
if $REBUILD && [[ -d "$BUILD_DIR" ]]; then
    echo ">> rm -rf $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo ">> cmake --preset $PRESET"
cmake --preset "$PRESET"

echo ">> cmake --build --preset $PRESET"
cmake --build --preset "$PRESET"

BIN="${BUILD_DIR}/cpufb"
if [[ -f "$BIN" ]]; then
    echo "Built: $BIN"
fi

if $PUSH; then
    cmake --build --preset "$PRESET" --target push_android
fi

if [[ -n "$RUN_CORE" ]]; then
    case "$RUN_CORE" in
        0|1|7) cmake --build --preset "$PRESET" --target "run_android_core${RUN_CORE}" ;;
        *)
            mask=$(( 1 << RUN_CORE ))
            adb shell taskset "$mask" /data/local/tmp/cpufb --thread_pool="[${RUN_CORE}]"
            ;;
    esac
fi
