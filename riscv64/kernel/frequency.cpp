#include <unistd.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include <compute.hpp>

#ifdef __linux__
#include <sys/syscall.h>
#endif

using namespace std;
vector<double> freq;

static void *thread_function_freq(void *arg)
{
    FrequencyData *data = new FrequencyData();
    double CPU_freq;
    int64_t looptime = 100000000;
    struct timespec start, end;
    double time_used;

#ifdef __linux__
    PerfEventCycle pec;
    int cpuid = *((int *)arg);
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
    if (read_freq == 0) {
        read_data(cpuid, &read_freq, "/cpufreq/cpuinfo_max_freq");
    }
    data->theory_freq = double(read_freq) * 1e-6;
//warm up
#ifdef _VECTOR_
    vector_vfmacc_vv_f64f64f64(looptime);
#endif
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pec.start();
#ifdef _VECTOR_
    vector_vfmacc_vv_f64f64f64(looptime);
#endif
    pec.stop();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    long long cycles = pec.get_cycle();

    if (cycles == 0) {
        CPU_freq = read_freq * 1e3;
    } else {
        CPU_freq = (double)cycles / time_used;
    }

    data->caculate_freq = CPU_freq * 1e-9;

#ifdef _VECTOR_
    const size_t vector_bytes = riscv_vector_length_bytes();
    const size_t workset_bytes = 16 * 1024;
    const int64_t repetitions = 16384;
    void *load_buffer = nullptr;
    if (vector_bytes > 0 &&
        posix_memalign(&load_buffer, 64, workset_bytes) == 0) {
        memset(load_buffer, 1, workset_bytes);
        vector_load_stream(load_buffer, workset_bytes, repetitions);
        PerfEventCycle load_counter(0, false);
        load_counter.start();
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        vector_load_stream(load_buffer, workset_bytes, repetitions);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        load_counter.stop();
        const double load_instructions =
            static_cast<double>(workset_bytes) / vector_bytes * repetitions;
        const long long load_cycles = load_counter.get_cycle();
        const double load_seconds = get_time(&start, &end);
        data->load_uses_perf_counter = load_cycles > 0;
        data->IPC_load = load_cycles > 0
            ? load_instructions / load_cycles
            : load_instructions / (load_seconds * CPU_freq);
        free(load_buffer);
    }
#endif
#endif

//  warm up
//warm up
#ifdef _VECTOR_
    vector_vfmacc_vv_f64f64f64(looptime);
#endif

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#ifdef _VECTOR_
    vector_vfmacc_vv_f64f64f64(looptime);
#endif
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp64 = looptime * 24 / (time_used * CPU_freq);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#ifdef _VECTOR_
    vector_vfmacc_vv_f32f32f32(looptime);
#endif
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp32 = looptime * 24 / (time_used * CPU_freq);

    pthread_exit((void *)data);
}

// TODO: clearer variable names
void get_cpu_freq(std::vector<int> &set_of_threads, Table &table)
{
    int num_thread = set_of_threads.size();
    void *thread_result;
    FrequencyData *result;
    freq.resize(num_thread);

    pthread_t threads[num_thread];
    int i = 0;
    for (int i = 0; i < num_thread; i++) {
        pthread_create(&threads[i], nullptr, thread_function_freq,
            (void *)&set_of_threads[i]);
    }
#ifndef __APPLE__
    for (int t = 0; t < num_thread; t++) {
        pthread_join(threads[t], &thread_result);
        result = (struct FrequencyData *)thread_result;
        stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
        ss1 << std::setprecision(2) << result->theory_freq << " GHZ";
        ss2 << std::setprecision(2) << result->caculate_freq << " GHZ";
        ss3 << std::setprecision(2) << result->IPC_fp32;
        ss4 << std::setprecision(2) << result->IPC_fp64;
        ss5 << std::setprecision(2) << result->IPC_load;
#ifdef _SVE_
        ss6 << std::setprecision(2) << result->IPC_fp32_sve;
        ss7 << std::setprecision(2) << result->IPC_fp64_sve;
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
        cont[6] = result->load_uses_perf_counter ? "perf_event cycles" :
            "clock/frequency estimate";
#ifdef _SVE_
        cont[6] = ss6.str();
        cont[7] = ss7.str();
#endif
        table.addOneItem(cont);
    delete result;
    }
#else
    for (int t = 0; t < num_thread; t++) {
        pthread_join(threads[t], &thread_result);
    }
    result = (struct FrequencyData *)thread_result;
    stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
    ss1 << std::setprecision(2) << result->theory_freq << " GHZ";
    ss2 << std::setprecision(2) << result->caculate_freq << " GHZ";
    ss3 << std::setprecision(2) << result->IPC_fp32;
    ss4 << std::setprecision(2) << result->IPC_fp64;
    ss5 << std::setprecision(2) << result->IPC_load;
#ifdef _SVE_
    ss6 << std::setprecision(2) << result->IPC_fp32_sve;
    ss7 << std::setprecision(2) << result->IPC_fp64_sve;
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
#ifdef _SVE_
    cont[6] = ss6.str();
    cont[7] = ss7.str();
#endif
    table.addOneItem(cont);
    delete result;
#endif
}
