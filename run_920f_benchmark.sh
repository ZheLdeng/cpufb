#!/bin/bash
#DSUB -n cpufb-920f
#DSUB -q q_pacopt
#DSUB -N 1
#DSUB -rpn 1
#DSUB -o cpufb-920f.%A.out
#DSUB -e cpufb-920f.%A.err

source /work_ssd/software/HPCKit/26.1.RC1/setvars.sh >/dev/null
set -euo pipefail

cd "$(dirname "$0")"
echo "hostname=$(hostname)"
uname -a
lscpu
grep -m1 '^Features' /proc/cpuinfo || true
for cache in /sys/devices/system/cpu/cpu0/cache/index*; do
    [ -f "$cache/level" ] || continue
    printf 'cache level=%s type=%s size=%s line=%s ways=%s shared=%s\n' \
        "$(cat "$cache/level")" "$(cat "$cache/type")" "$(cat "$cache/size")" \
        "$(cat "$cache/coherency_line_size")" "$(cat "$cache/ways_of_associativity")" \
        "$(cat "$cache/shared_cpu_list")"
done

cmake --preset native-release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build --preset native-release -j 1

rm -rf results-kunpeng920f-v2
python3 tools/run_statistical_trials.py run \
    --trials=10 --output-dir=results-kunpeng920f-v2 -- \
    taskset -c 0 build/native-release/cpufb '--thread_pool=[0]' --mode=all