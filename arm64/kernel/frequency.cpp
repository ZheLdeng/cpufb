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

#ifdef _SVE_
#include <arm_sve.h>
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

static void* thread_function_freq(void* arg){
    struct FrequencyData* data = (FrequencyData*)malloc(sizeof(FrequencyData));
    double CPU_freq = 0;
#ifdef __APPLE__
    int64_t looptime = 20000000;
#else
    int64_t looptime = 100000000;
#endif
    struct timespec start, end;
    double time_used;

#ifdef __APPLE__
    // Prefer real cycle counts from the private kperf fixed counters (the
    // common case on Apple Silicon).  powermetrics is the root-only sampled
    // fallback and the hard-coded per-model clock is the last resort.
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    MacosCounters counters;
    MacosCounterSnapshot start_counters, end_counters;
    bool use_kperf = counters.read(start_counters);

    char cpuType[256];
    size_t size = sizeof(cpuType);

    // 获取 CPU 架构名称
    if (sysctlbyname("machdep.cpu.brand_string", &cpuType, &size, NULL, 0) == -1) {
        perror("sysctl");
    }
    if(std::string(cpuType) == "Apple M1"){
        data->theory_freq = 3.2;
        data->caculate_freq = 3.2;
        CPU_freq = 3.2 * 1e9;
    } else if (std::string(cpuType) == "Apple M2"){
        data->theory_freq = 3.5;
        data->caculate_freq = 3.5;
        CPU_freq = 3.5 * 1e9;
    } else if (std::string(cpuType) == "Apple M3"){
        data->theory_freq = 4.06;
        data->caculate_freq = 4.06;
        CPU_freq = 4.06 * 1e9;
    } else if (std::string(cpuType) == "Apple M4 Pro"){
        data->theory_freq = 4.5;
        data->caculate_freq = 4.5;
        CPU_freq = 4.5 * 1e9;
    } else if (std::string(cpuType) == "Apple M5 Pro"){
        data->theory_freq = 4.6;
        data->caculate_freq = 4.6;
        CPU_freq = 4.6 * 1e9;
    }

#endif
#ifdef __linux__
    PerfEventCycle pec;
    int cpuid =* ((int *)arg);
    // Set affinity to the specified core
    cpu_set_t cpuset;
    pid_t pid = syscall(SYS_gettid);
    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpuid);
        printf("Warning: performance may be impacted \n");
    }
    //get CPU frequency
    data->theory_freq = 0;
    int read_freq = 0;
    read_data(cpuid, &read_freq, "/cpufreq/scaling_max_freq");
    if(read_freq == 0){
        read_data(cpuid, &read_freq, "/cpufreq/cpuinfo_max_freq");
    }
    const double fallback_ghz = cpu_freq_override_ghz();
    // cout << read_freq << endl;
    data->theory_freq = fallback_ghz > 0.0 ? fallback_ghz :
        double(read_freq) * 1e-6;
    //warm up
    asimd_fmla_vv_f64f64f64(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pec.start();
    asimd_fmla_vv_f64f64f64(looptime);
    pec.stop();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    long long cycles = pec.get_cycle();



    if(cycles == 0){
        CPU_freq = fallback_ghz > 0.0 ? fallback_ghz * 1e9 :
            read_freq * 1e3;
    }else{
        CPU_freq = (double)cycles / time_used;
    }

    data->caculate_freq = CPU_freq * 1e-9;
    // cout << data->caculate_freq  << endl;
#endif

    //  待补充 注释，warm up
    //warm up
    asimd_fmla_vv_f64f64f64(looptime);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#ifdef __APPLE__
    if (use_kperf) use_kperf = counters.read(start_counters);
#endif
    asimd_fmla_vv_f64f64f64(looptime);
#ifdef __APPLE__
    if (use_kperf) use_kperf = counters.read(end_counters);
#endif
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
#ifdef __APPLE__
    if (use_kperf && end_counters.cycles > start_counters.cycles) {
        CPU_freq = static_cast<double>(end_counters.cycles - start_counters.cycles) / time_used;
        data->caculate_freq = CPU_freq * 1e-9;
        data->counter_source = "kperf fixed counters";
    } else {
        double frequency_mhz = 0;
        std::string error;
        if (sample_powermetrics_frequency_mhz(frequency_mhz, error)) {
            CPU_freq = frequency_mhz * 1e6;
            data->caculate_freq = frequency_mhz * 1e-3;
            data->counter_source = "powermetrics estimate";
        } else {
            data->counter_source = "unavailable (" + error + ")";
        }
    }
#endif
    data->IPC_fp64 = CPU_freq > 0
        ? looptime * 24 / (time_used * CPU_freq) : 0;

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    asimd_fmla_vv_f32f32f32(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp32 = CPU_freq > 0
        ? looptime * 24 / (time_used * CPU_freq) : 0;

    float* cache_data = (float*)malloc(1024);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_ldr_kernel(cache_data, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_load = CPU_freq > 0
        ? looptime * 24 / (time_used * CPU_freq) : 0;

#ifdef _SVE_
    data->IPC_fp32_sve = 0;
    data->IPC_fp64_sve = 0;
    if (arm64_runtime_features().sve) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        sve_fmla_vv_f32f32f32(looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        time_used = get_time(&start, &end);
        data->IPC_fp32_sve = CPU_freq > 0
            ? looptime * 24 / (time_used * CPU_freq) : 0;

        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        sve_fmla_vv_f64f64f64(looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        time_used = get_time(&start, &end);
        data->IPC_fp64_sve = CPU_freq > 0
            ? looptime * 24 / (time_used * CPU_freq) : 0;
    }
#endif
    pthread_exit((void *)data);
}

// 变量名待修改
void get_cpu_freq(std::vector<int> &set_of_threads,Table &table)
{
    int num_thread = set_of_threads.size();
    void *thread_result;
    FrequencyData *result;
    freq.resize(num_thread);

    pthread_t threads[num_thread];
    int i = 0;
    for (int i = 0; i<num_thread; i++){
        pthread_create(&threads[i], NULL, thread_function_freq,  (void*)&set_of_threads[i] );
    }
#ifndef __APPLE__
    for (int t = 0; t < num_thread; t++) {
        pthread_join(threads[t], &thread_result);
        result = (struct FrequencyData *)thread_result;
        stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
        if (result->theory_freq > 0)
            ss1 << std::setprecision(2) << result->theory_freq <<" GHZ";
        else ss1 << "-";
        if (result->caculate_freq > 0) {
            ss2 << std::setprecision(2) << result->caculate_freq <<" GHZ";
            ss3 << std::setprecision(2) << result->IPC_fp32;
            ss4 << std::setprecision(2) << result->IPC_fp64;
            ss5 << std::setprecision(2) << result->IPC_load;
        } else {
            ss2 << "-"; ss3 << "-"; ss4 << "-"; ss5 << "-";
        }
        #ifdef _SVE_
        if (arm64_runtime_features().sve) {
            if (result->caculate_freq > 0) {
                ss6 << std::setprecision(2) << result->IPC_fp32_sve;
                ss7 << std::setprecision(2) << result->IPC_fp64_sve;
            } else {
                ss6 << "-";
                ss7 << "-";
            }
        } else {
            ss6 << "-";
            ss7 << "-";
        }
        #endif
        freq[t] = result->caculate_freq;
        vector<string> cont;
        cont.resize(table.getCol());
        cont[0] = to_string(set_of_threads[t]);
        cont[1] = ss1.str();
        cont[2] = ss2.str();
        cont[3] = ss3.str();
        cont[4] = ss4.str();
        cont[5] = ss5.str();
        cont[6] = "perf_event cycles";
        #ifdef _SVE_
        cont[7] = ss6.str();
        cont[8] = ss7.str();
        #endif
        table.addOneItem(cont);
    }
#else
    for (int t = 0; t < num_thread; t++) {
        pthread_join(threads[t], &thread_result);
    }
    result = (struct FrequencyData *)thread_result;
    stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
    if (result->theory_freq > 0)
        ss1 << std::setprecision(2) << result->theory_freq <<" GHZ";
    else ss1 << "-";
    if (result->caculate_freq > 0) {
        ss2 << std::setprecision(2) << result->caculate_freq <<" GHZ";
        ss3 << std::setprecision(2) << result->IPC_fp32;
        ss4 << std::setprecision(2) << result->IPC_fp64;
        ss5 << std::setprecision(2) << result->IPC_load;
    } else {
        ss2 << "-"; ss3 << "-"; ss4 << "-"; ss5 << "-";
    }
    #ifdef _SVE_
    ss6 << std::setprecision(2) << result->IPC_fp32_sve ;
    ss7 << std::setprecision(2) << result->IPC_fp64_sve ;
    #endif
    freq[0] = result->caculate_freq;
    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = "p-core";
    cont[1] = ss1.str();
    cont[2] = ss2.str();
    cont[3] = ss3.str();
    cont[4] = ss4.str();
    cont[5] = ss5.str();
    cont[6] = result->counter_source;
    #ifdef _SVE_
    cont[7] = ss6.str();
    cont[8] = ss7.str();
    #endif
    table.addOneItem(cont);
#endif

}
