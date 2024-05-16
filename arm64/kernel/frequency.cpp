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
uint64_t freq = 0;

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}


void get_cpu_freq(std::vector<int> &set_of_threads)
{
    //get CPU frequency
#ifdef __linux__
    FILE *fp = NULL;
    char buf[100]={0};
    string read_freq = "cat /sys/devices/system/cpu/cpu"+ std::to_string(set_of_threads[0]) +"/cpufreq/scaling_max_freq";
    fp = popen(read_freq.c_str(), "r");
    if(fp) {
        int ret = fread(buf,1,sizeof(buf)-1,fp);
        pclose(fp);
        freq = std::stoull(buf);
        return;
    } 
#endif
    long long count,looptime=10000000;
    struct timespec start,end;
    PerfEventCycle pec;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    pec.start();
    asimd_fmla_vv_f64f64f64(looptime);
    pec.stop();
    count=pec.get_cycle();
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    double time_used = get_time(&start, &end);
    freq = (double)count/time_used * 1e-3;
    // printf("time = %.10f,  CPU_freq = %.2f\n", time_used,(double)count/time_used);
    return;
}