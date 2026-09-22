#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <x86intrin.h>

#include "common.hpp"
#include "frequency.hpp"

#ifdef __linux__
#include <sys/syscall.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

using namespace std;

vector<double> freq;
static string freq_counter_source = "unavailable";

namespace {

const int64_t kFrequencyLoopTime = 100000000;
const double kInstructionsPerLoop = 16.0;

typedef void (*FrequencyKernel)(int64_t);

struct RatedFrequency
{
    double base_ghz = 0.0;
    double turbo_ghz = 0.0;
};

#ifdef __linux__
static uint16_t read_u16_le(const unsigned char *data)
{
    return static_cast<uint16_t>(data[0]) |
        (static_cast<uint16_t>(data[1]) << 8);
}

static RatedFrequency read_smbios_frequency()
{
    RatedFrequency frequency;
    std::ifstream input(
        "/sys/firmware/dmi/entries/4-0/raw", std::ios::in | std::ios::binary);
    unsigned char record[24] = {0};
    if (!input.read(reinterpret_cast<char *>(record), sizeof(record)) ||
        record[0] != 4 || record[1] < sizeof(record))
        return frequency;

    frequency.turbo_ghz = read_u16_le(record + 0x14) * 1e-3;
    frequency.base_ghz = read_u16_le(record + 0x16) * 1e-3;
    return frequency;
}

static RatedFrequency read_rated_frequency(int cpu_id)
{
    int base_frequency_khz = 0;
    int turbo_frequency_khz = 0;
    read_data(cpu_id, &base_frequency_khz, "/cpufreq/base_frequency");
    read_data(cpu_id, &turbo_frequency_khz, "/cpufreq/cpuinfo_max_freq");
    if (turbo_frequency_khz == 0)
        read_data(cpu_id, &turbo_frequency_khz, "/cpufreq/scaling_max_freq");

    RatedFrequency frequency;
    frequency.base_ghz = base_frequency_khz * 1e-6;
    frequency.turbo_ghz = turbo_frequency_khz * 1e-6;
    if (frequency.base_ghz == 0.0 || frequency.turbo_ghz == 0.0) {
        const RatedFrequency smbios = read_smbios_frequency();
        if (frequency.base_ghz == 0.0) frequency.base_ghz = smbios.base_ghz;
        if (frequency.turbo_ghz == 0.0) frequency.turbo_ghz = smbios.turbo_ghz;
    }
    return frequency;
}
#endif

static double measure_kernel(FrequencyKernel kernel, int64_t loop_time)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    kernel(loop_time);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    return get_time(&start, &end);
}

static double instruction_rate(double elapsed, double cycle_frequency)
{
    if (elapsed <= 0.0 || cycle_frequency <= 0.0) return 0.0;
    return kFrequencyLoopTime * kInstructionsPerLoop /
        (elapsed * cycle_frequency);
}

// One dependent ADD retires per core cycle, so the chain length over the
// elapsed time is the running core clock.  The best of three runs rejects
// interrupts, which can only lengthen a run.
static double add_chain_frequency()
{
    const int64_t loop_time = kFrequencyLoopTime / 4;
    volatile uint64_t addend = 3;
    cpufb_x64_frequency_add_chain(loop_time, addend);
    double best = 0.0;
    for (int repeat = 0; repeat < 3; ++repeat) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        cpufb_x64_frequency_add_chain(loop_time, addend);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        const double elapsed = get_time(&start, &end);
        if (elapsed > 0.0 && (best == 0.0 || elapsed < best)) best = elapsed;
    }
    return best > 0.0 ? loop_time * kInstructionsPerLoop / best : 0.0;
}

// Same contract as the ARM64 backend: a documented fixed core clock for hosts
// that deny unprivileged PMU access.
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

static void *thread_function_freq(void *arg)
{
    FrequencyData *data = new FrequencyData();
    double tsc_frequency = 0.0;
    double cycle_frequency = 0.0;

#ifdef __APPLE__
    (void)arg;
    // Intel Macs publish the nominal maximum; nothing is assumed per model.
    uint64_t max_frequency_hz = 0;
    size_t size = sizeof(max_frequency_hz);
    if (sysctlbyname(
            "hw.cpufrequency_max", &max_frequency_hz, &size, nullptr, 0) == 0)
        data->theory_freq = static_cast<double>(max_frequency_hz) * 1e-9;
#endif
#ifdef __linux__
    const int cpu_id = *static_cast<int *>(arg);

    cpu_set_t cpuset;
    const pid_t pid = syscall(SYS_gettid);
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted\n");
    }

    int max_frequency_khz = 0;
    read_data(cpu_id, &max_frequency_khz, "/cpufreq/scaling_max_freq");
    data->theory_freq = static_cast<double>(max_frequency_khz) * 1e-6;
#endif

    // Count real core cycles around the always-built SSE2 kernel when the PMU
    // is accessible, and calibrate the invariant TSC in the same window.  The
    // TSC ticks at a fixed reference rate that ignores turbo and AVX licence
    // changes, so it is only the last measured normalizer.  LFENCE keeps both
    // TSC reads outside the measured kernel body.
#ifdef __linux__
    PerfEventCycle cycle_counter(0, false);
#endif
    cpufb_x64_frequency_fsu64(kFrequencyLoopTime);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#ifdef __linux__
    cycle_counter.start();
#endif
    _mm_lfence();
    const uint64_t start_tsc = __rdtsc();
    cpufb_x64_frequency_fsu64(kFrequencyLoopTime);
    _mm_lfence();
    const uint64_t end_tsc = __rdtsc();
    long long cycles = 0;
#ifdef __linux__
    cycle_counter.stop();
    cycles = cycle_counter.get_cycle();
#endif
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    const double elapsed = get_time(&start, &end);
    if (elapsed > 0.0)
        tsc_frequency = static_cast<double>(end_tsc - start_tsc) / elapsed;
    data->tsc_freq = tsc_frequency * 1e-9;

    // Same order as ARM64: counted cycles, an explicit user clock, then the
    // estimates.  Only measured clocks are shown as "Test Freq".
    const double override_ghz = cpu_freq_override_ghz();
    bool measured = true;
    if (cycles > 0 && elapsed > 0.0) {
        cycle_frequency = static_cast<double>(cycles) / elapsed;
        data->counter_source = "perf_event cycles";
    } else if (override_ghz > 0.0) {
        cycle_frequency = override_ghz * 1e9;
        data->counter_source = "CPUFB_FREQ_GHZ (not measured)";
        measured = false;
    } else {
        const double chain_frequency = add_chain_frequency();
        if (chain_frequency > 0.0) {
            cycle_frequency = chain_frequency;
            data->counter_source = "ADD-chain estimate";
        } else if (tsc_frequency > 0.0) {
            cycle_frequency = tsc_frequency;
            data->counter_source = "invariant TSC";
        } else if (data->theory_freq > 0.0) {
            cycle_frequency = data->theory_freq * 1e9;
            data->counter_source = "OS-reported frequency (not measured)";
            measured = false;
        }
    }
    data->clock_ghz = cycle_frequency * 1e-9;
    data->caculate_freq = measured ? data->clock_ghz : 0.0;
    if (data->caculate_freq > 0.0 && data->theory_freq > 0.0 &&
        data->caculate_freq < 0.85 * data->theory_freq)
        data->counter_source += "; reported maximum not sustained under load";

    // CPUFB_DEBUG_CLOCK=1 prints every available clock next to the selected
    // one, to cross-check the ADD-chain estimate against counted cycles.
    const char *debug_clock = getenv("CPUFB_DEBUG_CLOCK");
    if (debug_clock != nullptr && *debug_clock != '\0' &&
        std::string(debug_clock) != "0") {
        fprintf(stderr,
            "clock debug: selected=%.4f GHz (%s) counted=%.4f GHz "
            "add_chain=%.4f GHz tsc=%.4f GHz reported=%.4f GHz\n",
            data->clock_ghz, data->counter_source.c_str(),
            cycles > 0 && elapsed > 0.0 ? cycles / elapsed * 1e-9 : 0.0,
            add_chain_frequency() * 1e-9, data->tsc_freq, data->theory_freq);
    }

    cpufb_x64_frequency_fsu64(kFrequencyLoopTime);
    data->IPC_fp64 = instruction_rate(
        measure_kernel(cpufb_x64_frequency_fsu64, kFrequencyLoopTime),
        cycle_frequency);

    cpufb_x64_frequency_fsu32(kFrequencyLoopTime);
    data->IPC_fp32 = instruction_rate(
        measure_kernel(cpufb_x64_frequency_fsu32, kFrequencyLoopTime),
        cycle_frequency);

    alignas(64) float cache_data[16] = {0.0f};
    cpufb_x64_frequency_load(cache_data, kFrequencyLoopTime);
    struct timespec load_start, load_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &load_start);
    cpufb_x64_frequency_load(cache_data, kFrequencyLoopTime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &load_end);
    data->IPC_load =
        instruction_rate(get_time(&load_start, &load_end), cycle_frequency);

    return data;
}

} // namespace

void get_cpu_freq(std::vector<int> &set_of_threads, Table &table)
{
    const size_t num_threads = set_of_threads.size();
    freq.resize(num_threads);
    vector<pthread_t> threads(num_threads);

    for (size_t i = 0; i < num_threads; ++i)
        pthread_create(&threads[i], nullptr, thread_function_freq,
            static_cast<void *>(&set_of_threads[i]));

    for (size_t i = 0; i < num_threads; ++i) {
        void *thread_result = nullptr;
        pthread_join(threads[i], &thread_result);
        FrequencyData *result = static_cast<FrequencyData *>(thread_result);

        stringstream theory_freq, measured_freq, fsu32, fsu64, load;
        if (result->theory_freq > 0.0)
            theory_freq << setprecision(2) << result->theory_freq << " GHZ";
        else
            theory_freq << "-";
        if (result->caculate_freq > 0.0)
            measured_freq << setprecision(3) << result->caculate_freq << " GHZ";
        else
            measured_freq << "-";
        if (result->clock_ghz > 0.0) {
            fsu32 << setprecision(2) << result->IPC_fp32;
            fsu64 << setprecision(2) << result->IPC_fp64;
            load << setprecision(2) << result->IPC_load;
        } else {
            fsu32 << "-";
            fsu64 << "-";
            load << "-";
        }
        freq[i] = result->clock_ghz;
        if (i == 0) freq_counter_source = result->counter_source;

        vector<string> row(table.getCol());
        row[0] = to_string(set_of_threads[i]);
        row[1] = theory_freq.str();
        row[2] = measured_freq.str();
        row[3] = fsu32.str();
        row[4] = fsu64.str();
        row[5] = load.str();
        row[6] = result->counter_source;
        table.addOneItem(row);

        delete result;
    }
}

const std::string &cpu_freq_counter_source()
{
    return freq_counter_source;
}
