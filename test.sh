#!/usr/bin/env bash
set -euo pipefail

CPU=7
CPU_MASK=80
DEVICE_BIN=/data/local/tmp/cpufb
DEVICE_LOG=/data/local/tmp/cpufb-oneplus-pmb110-core7-all.txt
HOST_LOG=/tmp/cpufb-oneplus-pmb110-mt6993-core7-all-$(date +%Y%m%d-%H%M%S).txt

cd /Users/jtmeng/Documents/cpufb_BDMA/cpufb

# 构建 Android arm64-v8a 版本；包含设备已确认支持的基础 SME 内核。
cmake -S . -B build/android-arm64-sme \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/opt/homebrew/share/android-ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-36 \
  '-DCPUFB_SIMD_FEATURES_OVERRIDE=_ASIMD_;_ASIMD_HP_;_ASIMD_DP_;_BF16_;_I8MM_;_FHM_;_ASIMD_FCMA_;_ASIMD_REDUCE_;_ASIMD_RECIP_;_ASIMD_INT_MAC_;_ASIMD_TBL_;_SVE_;_SVE_I8MM_;_SVE_BF16_;_SVE_FP16_FMLA_;_SVE2_;_SME_;_LDP_;_ISSUE_'

cmake --build build/android-arm64-sme -j 4

# 部署到手机。
adb push build/android-arm64-sme/cpufb "$DEVICE_BIN"
adb shell "chmod 755 '$DEVICE_BIN'"

# 先确认设备的 SME/SVE 特性。
adb shell 'grep -m1 "^Features" /proc/cpuinfo'
adb shell 'for policy in /sys/devices/system/cpu/cpufreq/policy*; do
  [ -d "$policy" ] || continue
  printf "%s CPUs=" "${policy##*/}"
  cat "$policy/related_cpus"
  printf "max_kHz="
  cat "$policy/cpuinfo_max_freq"
done'

# 执行完整缓存与指令集测试，并在手机端可靠保存日志。
adb shell "taskset $CPU_MASK '$DEVICE_BIN' '--thread_pool=[$CPU]' --mode=all > '$DEVICE_LOG' 2>&1"

# 拉回并显示完整输出。
adb pull "$DEVICE_LOG" "$HOST_LOG"
cat "$HOST_LOG"

echo "Saved log: $HOST_LOG"
