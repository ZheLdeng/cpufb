#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// void load_ldp_kernel(float*);
#include<time.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int main(){
    struct perf_event_attr pe;
    long long count, count1, count2;
    int fd;
    int64_t looptime= 100000000;
    int tmp=0;
    // Initialize perf_event_attr structure
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CPU_CYCLES; // Measure CPU cycles
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    // Open a file descriptor to the PMU
    fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        perror("perf_event_open");
        exit(EXIT_FAILURE);
    }

    // Enable the PMU
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        struct timespec start, end;
    double time_used;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    count1 = syscall(SYS_gettid);

    
    for(int i=0;i<looptime;i++){
        tmp++;
    }

    count2 = syscall(SYS_gettid);

    // Disable the PMU
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

    // Read the number of CPU cycles
    read(fd, &count, sizeof(long long));
    printf("CPU cycles: %lld\n", count);

    close(fd);

    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    // printf("use time %s  %f\n", item.isa, time_used);

    printf("time = %.10f,  CPU_freq = %.2f\n", time_used,(double)count/time_used);
    return 0;
}