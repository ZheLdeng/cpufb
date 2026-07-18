#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
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

namespace {

const int64_t kFrequencyLoopTime = 100000000;
const double kInstructionsPerLoop = 16.0;

typedef void (*FrequencyKernel)(int64_t);

static double measure_kernel(FrequencyKernel kernel, int64_t loop_time)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    kernel(loop_time);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    return get_time(&start, &end);
}

static double instruction_rate(double elapsed, double tsc_frequency)
{
    if (elapsed <= 0.0 || tsc_frequency <= 0.0) return 0.0;
    return kFrequencyLoopTime * kInstructionsPerLoop /
        (elapsed * tsc_frequency);
}

static void* thread_function_freq(void* arg)
{
    FrequencyData* data = new FrequencyData();
    double tsc_frequency = 0.0;

#ifdef __APPLE__
    char cpu_type[256] = {0};
    size_t size = sizeof(cpu_type);
    if (sysctlbyname("machdep.cpu.brand_string", cpu_type, &size, NULL, 0) == -1) {
        perror("sysctl");
    }
    if (std::string(cpu_type) == "Apple M1") {
        data->theory_freq = 3.2;
        data->caculate_freq = 3.2;
        tsc_frequency = 3.2e9;
    } else if (std::string(cpu_type) == "Apple M2") {
        data->theory_freq = 3.5;
        data->caculate_freq = 3.5;
        tsc_frequency = 3.5e9;
    } else if (std::string(cpu_type) == "Apple M3") {
        data->theory_freq = 4.06;
        data->caculate_freq = 4.06;
        tsc_frequency = 4.06e9;
    }
#endif

#ifdef __linux__
    const int cpu_id = *static_cast<int*>(arg);

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

    // Calibrate the invariant TSC using the same always-built SSE2 kernel used
    // below.  LFENCE keeps both TSC reads outside the measured kernel body.
    cpufb_x64_frequency_fsu64(kFrequencyLoopTime);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    _mm_lfence();
    const uint64_t start_tsc = __rdtsc();
    cpufb_x64_frequency_fsu64(kFrequencyLoopTime);
    _mm_lfence();
    const uint64_t end_tsc = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    const double elapsed = get_time(&start, &end);
    if (elapsed > 0.0)
        tsc_frequency = static_cast<double>(end_tsc - start_tsc) / elapsed;
    data->caculate_freq = tsc_frequency * 1e-9;
#endif

    cpufb_x64_frequency_fsu64(kFrequencyLoopTime);
    data->IPC_fp64 = instruction_rate(
        measure_kernel(cpufb_x64_frequency_fsu64, kFrequencyLoopTime),
        tsc_frequency);

    cpufb_x64_frequency_fsu32(kFrequencyLoopTime);
    data->IPC_fp32 = instruction_rate(
        measure_kernel(cpufb_x64_frequency_fsu32, kFrequencyLoopTime),
        tsc_frequency);

    alignas(64) float cache_data[16] = {0.0f};
    cpufb_x64_frequency_load(cache_data, kFrequencyLoopTime);
    struct timespec load_start, load_end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &load_start);
    cpufb_x64_frequency_load(cache_data, kFrequencyLoopTime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &load_end);
    data->IPC_load = instruction_rate(get_time(&load_start, &load_end),
        tsc_frequency);

    return data;
}

}  // namespace

void get_cpu_freq(std::vector<int> &set_of_threads, Table &table)
{
    const size_t num_threads = set_of_threads.size();
    freq.resize(num_threads);
    vector<pthread_t> threads(num_threads);

    for (size_t i = 0; i < num_threads; ++i)
        pthread_create(&threads[i], NULL, thread_function_freq,
            static_cast<void*>(&set_of_threads[i]));

    for (size_t i = 0; i < num_threads; ++i) {
        void* thread_result = NULL;
        pthread_join(threads[i], &thread_result);
        FrequencyData* result = static_cast<FrequencyData*>(thread_result);

        stringstream theory_freq, measured_freq, fsu32, fsu64, load;
        theory_freq << setprecision(2) << result->theory_freq << " GHZ";
        measured_freq << setprecision(2) << result->caculate_freq << " GHZ";
        fsu32 << setprecision(2) << result->IPC_fp32;
        fsu64 << setprecision(2) << result->IPC_fp64;
        load << setprecision(2) << result->IPC_load;
        freq[i] = result->caculate_freq;

        vector<string> row(table.getCol());
        row[0] = to_string(set_of_threads[i]);
        row[1] = theory_freq.str();
        row[2] = measured_freq.str();
        row[3] = fsu32.str();
        row[4] = fsu64.str();
        row[5] = load.str();
        table.addOneItem(row);

        delete result;
    }
}
