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
#include <stdlib.h>
// #include <stdio.h>

#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include "load.hpp"

#ifdef __APPLE__
#include <sys/sysctl.h>
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
        ata->theory_freq = 3.2;
        data->caculate_freq = 3.2;
        CPU_freq = 3.2 * 1e9;
    }else if(std::string(cpuType) == "Apple M2"){
        data->theory_freq = 3.5;
        data->caculate_freq = 3.5;
        CPU_freq = 3.5 * 1e9;
    }else if(std::string(cpuType) == "Apple M3"){
        data->theory_freq = 4.06;
        data->caculate_freq = 4.06;
        CPU_freq = 4.06 * 1e9;
    }
    
#endif
#ifdef __linux__
    int cpuid =* ((int *)arg);

    // Set affinity to the specified core
    cpu_set_t cpuset;
    pid_t pid = gettid();
    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);

    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpuid);
        printf("Warning: performance may be impacted \n");
    }

    // PerfEventCycle pec;
    PerfEventCycle test_pec = PerfEventCycle(0);

    test_pec.start();
    volatile int counter = 0; 
    for (int i = 0; i < 10000; ++i) {
        counter += 1;
    }
    test_pec.stop();
    long long test_cycle = test_pec.get_cycle();

    PerfEventCycle pec = (test_cycle == 0) ? PerfEventCycle(1) : PerfEventCycle(0);


    //get CPU frequency
    FILE *fp = NULL;
    char buf[100] = {0};
    string file_path="/sys/devices/system/cpu/cpu"+ std::to_string(cpuid) +"/cpufreq/scaling_max_freq";
    std::ifstream file(file_path);
    if (file) {
        string read_freq = "cat " + file_path;
        fp = popen(read_freq.c_str(), "r");
        if (fp) {
            int ret = fread(buf, 1, sizeof(buf)-1, fp);
            if (ret > 0) {
                data->theory_freq = std::stod(buf) * 1e-6;
            }
            pclose(fp);
        }
    } else {
        data->theory_freq = 0;
    }
    //warm up
    sse2_add_mul_f64f64_f64(looptime, NULL);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pec.start();
    sse2_add_mul_f64f64_f64(looptime, NULL);
    pec.stop();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    CPU_freq = (double)pec.get_cycle() / time_used;
    data->caculate_freq = CPU_freq * 1e-9;
#endif

    //  待补充 注释，warm up
    //warm up
    sse2_add_mul_f64f64_f64(looptime, NULL);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    fma_f64f64f64(looptime, NULL);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp64 = looptime * 16 / (time_used * CPU_freq);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    fma_f32f32f32(looptime, NULL);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_fp32 = looptime * 16 / (time_used * CPU_freq);

    float* cache_data = (float*)malloc(1024);
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_movups_kernel(cache_data, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data->IPC_load = looptime * 16 / (time_used * CPU_freq);

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

    for (int t = 0; t < num_thread; t++) {
        pthread_join(threads[t], &thread_result);
        result = (struct FrequencyData *)thread_result;
        stringstream ss1, ss2, ss3, ss4, ss5, ss6, ss7;
        ss1 << std::setprecision(2) << result->theory_freq <<" GHZ" ;
        ss2 << std::setprecision(2) << result->caculate_freq <<" GHZ" ;
        ss3 << std::setprecision(2) << result->IPC_fp32 ;
        ss4 << std::setprecision(2) << result->IPC_fp64 ;
        ss5<< std::setprecision(2) << result->IPC_load ;
        freq[t] = result->caculate_freq;

        vector<string> cont;
        cont.resize(table.getCol());
        cont[0] = to_string(set_of_threads[t]);
        cont[1] = ss1.str();
        cont[2] = ss2.str();
        cont[3] = ss3.str();
        cont[4] = ss4.str();
        cont[5] = ss5.str();

        table.addOneItem(cont);
    }

}
