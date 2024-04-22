#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}
void load_ldp_kernel(float*, int ,int);

int start_perf_monitoring(struct perf_event_attr *pe, int *fd) {
    *fd = perf_event_open(pe, 0, -1, -1, 0);
    if (*fd == -1) {
        perror("perf_event_open failed");
        return -1;
    }

    ioctl(*fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(*fd, PERF_EVENT_IOC_ENABLE, 0);
    return 0;
}

int main() {
    struct timespec start, end;
    struct perf_event_attr pe;
    long long count;
    int fd;
    // Configure the perf_event for L1 cache load misses
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HW_CACHE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config =  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16)) ;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    double time_used;
    int long long data_size = 128 * 1024;
    float* data = (float*)malloc(data_size);
    
    // memset(tail, 0, 4);

    int long long looptime=1000000;
    int inner_loop = data_size / sizeof(float) / (4 * 32);
    for (int i = 0; i < data_size/sizeof(float); i++) {
        data[i] = i;
        // data2[i] = i;
    }
    // load_ldp_kernel(data, inner_loop, looptime);
    if (start_perf_monitoring(&pe, &fd) != 0) {
        exit(EXIT_FAILURE);
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    load_ldp_kernel(data, inner_loop, looptime);
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);

    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    read(fd, &count, sizeof(long long));
    printf("L1 Cache Load : %lld\n", count);
    printf("Total load = %ld Byte,Total num =%d \n", looptime * data_size);
    close(fd);


    time_used = get_time(&start, &end);
    free(data);
    double perf = looptime * data_size /
        (time_used * 2.6 * 1e9);
    printf("time = %.10f, perf = %.2f byte/cycle \n", time_used, perf);
    return 0;
}