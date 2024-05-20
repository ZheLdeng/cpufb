#include <unistd.h>
#include <linux/perf_event.h>  
#include <sys/ioctl.h>  
#include <sys/syscall.h>  
#include <sys/types.h>   
#include <cstdio>  
#include <cstdlib>  
#include <ctime>  
#include <iostream>
#include <cstdint>
#include "compute.hpp"
#include "frequency.hpp"
#include "perf_event.hpp"
#include "load.hpp"

#include <string>  
#include <vector>

#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>

#include <stdlib.h>
#include <fcntl.h>


static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}
using namespace std;
vector<double> freq;

void* thread_function_freq(void* arg){
    int cpuid=*((int *)arg);
    // Set affinity to the specified core
    cpu_set_t cpuset;
    pid_t pid = gettid();
    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) < 0) {  
        printf("Error: cpu id %d sched_setaffinity\n", cpuid);  
        printf("Warning: performance may be impacted \n");  
    } 
    struct FrequencyData data;
    //get CPU frequency
#ifdef __linux__
    FILE *fp = NULL;
    char buf[100]={0};
    string read_freq = "cat /sys/devices/system/cpu/cpu"+ std::to_string(cpuid) +"/cpufreq/scaling_max_freq";
    fp = popen(read_freq.c_str(), "r");
    if(fp) {
        int ret = fread(buf,1,sizeof(buf)-1,fp);
        pclose(fp);
        data.theory_freq=std::stoull(buf) * 1e-6;
        // freq = std::stoull(buf);
        // return;
    } 
#endif
    PerfEventCycle pec;
    int64_t looptime= 100000000;
    struct timespec start, end;
    double time_used;

    //warm up
    asimd_fmla_vv_f64f64f64(looptime);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pec.start();
    asimd_fmla_vv_f64f64f64(looptime);
    pec.stop();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    double CPU_freq=(double)pec.get_cycle()/time_used;
    data.caculate_freq = CPU_freq;
    data.IPC_fp64 = looptime * 24 / (time_used * CPU_freq);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    asimd_fmla_vv_f32f32f32(looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data.IPC_fp32 = looptime * 24 / (time_used * CPU_freq);
    float* cache_data = (float*)malloc(1024);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_ldr_kernel(cache_data, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    data.IPC_load = looptime * 24 / (time_used * CPU_freq);
    pthread_exit((void *)&data);
}

void get_cpu_freq(std::vector<int> &set_of_threads,Table &table)
{
    int num_thread=set_of_threads.size();
    void *thread_result;
    FrequencyData *result;
    freq.resize(num_thread);

    pthread_t threads[num_thread];
    int i=0;
    for(int i = 0;i<num_thread;i++){
        pthread_create(&threads[i], NULL, thread_function_freq,  (void*)&set_of_threads[i] );
    }
    
    for (int t = 0; t < num_thread; t++) {
        pthread_join(threads[t], &thread_result);
        result = (FrequencyData *)thread_result;
        stringstream ss1, ss2, ss3, ss4, ss5;
        ss1 << std::setprecision(2) << result->theory_freq ;
        ss2 << std::setprecision(2) << result->caculate_freq * 1e-9 ;
        ss3 << std::setprecision(2) << result->IPC_fp32 ;
        ss4 << std::setprecision(2) << result->IPC_fp64 ;
        ss5<< std::setprecision(2) << result->IPC_load ;

        freq[t]=result->theory_freq;

        vector<string> cont;
        cont.resize(table.getCol());
        cont[0] =to_string(set_of_threads[t]);
        cont[1] = ss1.str();
        cont[2] = ss2.str();
        cont[3] = ss3.str();
        cont[4] = ss4.str();
        cont[5] = ss5.str();
        table.addOneItem(cont);
    }

}