#include <unistd.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include "load.hpp"
#if defined(__linux__) && !defined(__APPLE__)
#include "../runtime_features.hpp"
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include "macos_counters.hpp"
#endif

#ifdef __linux__
#include <sys/syscall.h>
#endif

using namespace std;
vector<double> freq;

// Some virtualized Linux hosts deliberately deny unprivileged PMU cycle
// counters.  A caller can then supply a documented fixed core clock through
// CPUFB_FREQ_GHZ, rather than silently producing infinities for all
// cycle-normalized measurements.
static double cpu_freq_override_ghz()
{
    const char *value = getenv("CPUFB_FREQ_GHZ");
    if (value == nullptr || *value == '\0') return 0.0;

    char *end = nullptr;
    const double ghz = strtod(value, &end);
    if (end == value || *end != '\0' || !std::isfinite(ghz) || ghz <= 0.0)
        return 0.0;
    return ghz;
}

extern "C" uint64_t clock_add_chain(int64_t looptime, uint64_t addend);

typedef void (*FrequencyKernel)(int64_t);

static double time_kernel(FrequencyKernel kernel, int64_t looptime)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    kernel(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    return get_time(&start, &end);
}

// Core clock without PMU access: one dependent register-register ADD retires
// per cycle on every AArch64 core, so the chain length over the elapsed time
// is the clock of the core this thread is running on.  Interrupts can only
// lengthen a run, hence the best of three.
static double add_chain_frequency_hz()
{
    const int64_t looptime = 10000000;
    volatile uint64_t addend = 3;
    clock_add_chain(looptime, addend);
    double best = 0.0;
    for (int repeat = 0; repeat < 3; ++repeat) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        clock_add_chain(looptime, addend);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double elapsed = get_time(&start, &end);
        if (elapsed > 0.0 && (best == 0.0 || elapsed < best)) best = elapsed;
    }
    return best > 0.0 ? looptime * 16.0 / best : 0.0;
}

static double instruction_rate(
    int64_t looptime, double elapsed, double clock_hz)
{
    return clock_hz > 0.0 && elapsed > 0.0
        ? looptime * 24 / (elapsed * clock_hz)
        : 0.0;
}

// Clock sources, best first.  Only 1, 3 and 4 are measurements and fill
// the "Test Freq" column; a reported or user-supplied clock still normalizes
// IPC but is never displayed as if it had been measured.
//   1. PMU cycles around the FMLA kernel (perf_event on Linux, kperf fixed
//      counters on macOS; both usually need elevated privileges);
//   2. CPUFB_FREQ_GHZ, an explicit user-supplied clock (not measured);
//   3. the ADD dependency chain (always available, no privileges);
//   4. macOS only: a powermetrics sample (root);
//   5. the OS-reported maximum frequency (not measured).
static void *thread_function_freq(void *arg)
{
    // FrequencyData owns a std::string, so it must be constructed, not
    // malloc'd; get_cpu_freq() deletes it after the join.
    FrequencyData *data = new FrequencyData();
#ifdef __APPLE__
    const int64_t looptime = 20000000;
    (void)arg;
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    data->theory_freq = macos_reported_max_frequency_ghz();
#else
    const int64_t looptime = 100000000;
    const int cpuid = *static_cast<int *>(arg);
    cpu_set_t cpuset;
    pid_t pid = syscall(SYS_gettid);
    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpuid);
        printf("Warning: performance may be impacted \n");
    }
    int reported_khz = 0;
    read_data(cpuid, &reported_khz, "/cpufreq/scaling_max_freq");
    if (reported_khz == 0)
        read_data(cpuid, &reported_khz, "/cpufreq/cpuinfo_max_freq");
    data->theory_freq = reported_khz * 1e-6;
#endif

    // Warm up, then count cycles around the same kernel that is timed.
    asimd_fmla_vv_f64f64f64(looptime);
    double clock_hz = 0.0;
    struct timespec start, end;
#ifdef __APPLE__
    MacosCounters counters;
    MacosCounterSnapshot start_counters, end_counters;
    bool counted = counters.read(start_counters);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    asimd_fmla_vv_f64f64f64(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    counted = counted && counters.read(end_counters) &&
        end_counters.cycles > start_counters.cycles;
    const double counted_cycles = counted
        ? static_cast<double>(end_counters.cycles - start_counters.cycles)
        : 0.0;
    const char *counted_source = "kperf fixed counters";
#else
    PerfEventCycle cycle_counter(0, false);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    cycle_counter.start();
    asimd_fmla_vv_f64f64f64(looptime);
    cycle_counter.stop();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    const double counted_cycles =
        static_cast<double>(cycle_counter.get_cycle());
    const char *counted_source = "perf_event cycles";
#endif
    const double fp64_elapsed = get_time(&start, &end);

    if (counted_cycles > 0.0 && fp64_elapsed > 0.0) {
        clock_hz = counted_cycles / fp64_elapsed;
        data->caculate_freq = clock_hz * 1e-9;
        data->counter_source = counted_source;
    }
    // An explicit user clock outranks the estimates but is not a measurement.
    const double override_ghz = cpu_freq_override_ghz();
    if (clock_hz <= 0.0 && override_ghz > 0.0) {
        clock_hz = override_ghz * 1e9;
        data->counter_source = "CPUFB_FREQ_GHZ (not measured)";
    }
    if (clock_hz <= 0.0) {
        clock_hz = add_chain_frequency_hz();
        if (clock_hz > 0.0) {
            data->caculate_freq = clock_hz * 1e-9;
            data->counter_source = "ADD-chain estimate";
        }
    }
#ifdef __APPLE__
    if (clock_hz <= 0.0) {
        double frequency_mhz = 0;
        std::string error;
        if (sample_powermetrics_frequency_mhz(frequency_mhz, error)) {
            clock_hz = frequency_mhz * 1e6;
            data->caculate_freq = frequency_mhz * 1e-3;
            data->counter_source = "powermetrics estimate";
        }
    }
#endif
    if (clock_hz <= 0.0 && data->theory_freq > 0.0) {
        clock_hz = data->theory_freq * 1e9;
        data->counter_source = "OS-reported frequency (not measured)";
    }
    data->clock_ghz = clock_hz * 1e-9;

    data->IPC_fp64 = instruction_rate(looptime, fp64_elapsed, clock_hz);
    data->IPC_fp32 = instruction_rate(
        looptime, time_kernel(asimd_fmla_vv_f32f32f32, looptime), clock_hz);

    float *cache_data = nullptr;
    if (posix_memalign((void **)&cache_data, 64, 1024) == 0) {
        memset(cache_data, 0, 1024);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_ldr_kernel(cache_data, looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        data->IPC_load =
            instruction_rate(looptime, get_time(&start, &end), clock_hz);
        free(cache_data);
    }

#ifdef _SVE_
    if (arm64_runtime_features().sve) {
        data->IPC_fp32_sve = instruction_rate(
            looptime, time_kernel(sve_fmla_vv_f32f32f32, looptime), clock_hz);
        data->IPC_fp64_sve = instruction_rate(
            looptime, time_kernel(sve_fmla_vv_f64f64f64, looptime), clock_hz);
    }
#endif
    pthread_exit((void *)data);
}

static std::string format_value(double value, int precision, const char *unit)
{
    if (value <= 0.0) return "-";
    std::stringstream stream;
    stream << std::setprecision(precision) << value << unit;
    return stream.str();
}

static void append_frequency_row(
    Table &table, const std::string &core, const FrequencyData &result)
{
    // IPC needs some clock, measured or reported; "Test Freq" only ever shows
    // a measured one.
    const bool has_clock = result.clock_ghz > 0.0;
    vector<string> cont(table.getCol());
    cont[0] = core;
    cont[1] = format_value(result.theory_freq, 2, " GHZ");
    cont[2] = format_value(result.caculate_freq, 3, " GHZ");
    cont[3] = has_clock ? format_value(result.IPC_fp32, 2, "") : "-";
    cont[4] = has_clock ? format_value(result.IPC_fp64, 2, "") : "-";
    cont[5] = has_clock ? format_value(result.IPC_load, 2, "") : "-";
    cont[6] = result.counter_source;
#ifdef _SVE_
    cont[7] = has_clock ? format_value(result.IPC_fp32_sve, 2, "") : "-";
    cont[8] = has_clock ? format_value(result.IPC_fp64_sve, 2, "") : "-";
#endif
    table.addOneItem(cont);
}

void get_cpu_freq(std::vector<int> &set_of_threads, Table &table)
{
    const size_t num_thread = set_of_threads.size();
    freq.assign(num_thread, 0.0);

    vector<pthread_t> threads(num_thread);
    for (size_t i = 0; i < num_thread; i++)
        pthread_create(&threads[i], nullptr, thread_function_freq,
            (void *)&set_of_threads[i]);

    for (size_t t = 0; t < num_thread; t++) {
        void *thread_result = nullptr;
        pthread_join(threads[t], &thread_result);
        FrequencyData *result = static_cast<FrequencyData *>(thread_result);
        if (result == nullptr) continue;
#ifdef __APPLE__
        // macOS cannot pin threads, so the workers are interchangeable
        // performance-core samples: report one row and share its clock.
        if (t + 1 == num_thread) {
            freq[0] = result->clock_ghz;
            append_frequency_row(table, "p-core", *result);
        }
#else
        freq[t] = result->clock_ghz;
        append_frequency_row(table, to_string(set_of_threads[t]), *result);
#endif
        delete result;
    }
}
