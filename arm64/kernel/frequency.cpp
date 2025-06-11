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
#include "load.hpp"

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#ifdef _SVE_
#include <arm_sve.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
#endif

using namespace std;
vector<double> freq;

static void* thread_function_freq(void* arg){
    struct FrequencyData* data = (FrequencyData*)malloc(sizeof(FrequencyData));
    double CPU_freq;
    int64_t looptime = 100000000;
    struct timespec start, end;
    double time_used;

#ifdef __APPLE__
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
    // cout << "read " << endl;
    data->theory_freq = double(read_freq) * 1e-6;
    //warm up
    asimd_fmla_vv_f64f64f64(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pec.start();
    asimd_fmla_vv_f64f64f64(looptime);
    pec.stop();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    CPU_freq = (double)pec.get_cycle() / time_used;
    data->caculate_freq = CPU_freq * 1e-9;
#endif

    //  待补充 注释，warm up
    //warm up
    asimd_fmla_vv_f64f64f64(looptime);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    asimd_fmla_vv_f64f64f64(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp64 = looptime * 24 / (time_used * CPU_freq);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    asimd_fmla_vv_f32f32f32(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp32 = looptime * 24 / (time_used * CPU_freq);

    float* cache_data = (float*)malloc(1024);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_ldr_kernel(cache_data, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_load = looptime * 24 / (time_used * CPU_freq);

#ifdef _SVE_
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    sve_fmla_vv_f32f32f32(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp32_sve = looptime * 24 / (time_used * CPU_freq);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    sve_fmla_vv_f64f64f64(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp64_sve = looptime * 24 / (time_used * CPU_freq);
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
        ss1 << std::setprecision(2) << result->theory_freq <<" GHZ" ;
        ss2 << std::setprecision(2) << result->caculate_freq <<" GHZ" ;
        ss3 << std::setprecision(2) << result->IPC_fp32 ;
        ss4 << std::setprecision(2) << result->IPC_fp64 ;
        ss5<< std::setprecision(2) << result->IPC_load ;
        #ifdef _SVE_
        ss6 << std::setprecision(2) << result->IPC_fp32_sve ;
	    ss7 << std::setprecision(2) << result->IPC_fp64_sve ;
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
        #ifdef _SVE_
	    cont[6] = ss6.str();
	    cont[7] = ss7.str();
        #endif
        table.addOneItem(cont);
    }
#else
    for (int t = 0; t < num_thread; t++) {  
        pthread_join(threads[t], &thread_result);
    }
    result = (struct FrequencyData *)thread_result;
    stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
    ss1 << std::setprecision(2) << result->theory_freq <<" GHZ" ;
    ss2 << std::setprecision(2) << result->caculate_freq <<" GHZ" ;
    ss3 << std::setprecision(2) << result->IPC_fp32 ;
    ss4 << std::setprecision(2) << result->IPC_fp64 ;
    ss5<< std::setprecision(2) << result->IPC_load ;
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
    #ifdef _SVE_
    cont[6] = ss6.str();
    cont[7] = ss7.str();
    #endif
    table.addOneItem(cont);
#endif

}
