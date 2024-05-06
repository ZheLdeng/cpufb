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

int start_perf_monitoring(struct perf_event_attr *l1_access, int *fd) {
    *fd = perf_event_open(l1_access, 0, -1, -1, 0);
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
    struct perf_event_attr l1_access,l1_miss;
    long long count1,count2;
    int fd1,fd2;
    // Configure the perf_event for L1 cache load misses
    memset(&l1_access, 0, sizeof(struct perf_event_attr));
    l1_access.type = PERF_TYPE_HW_CACHE;
    l1_access.size = sizeof(struct perf_event_attr);
    l1_access.config =  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16)) ;
    l1_access.disabled = 1;
    l1_access.exclude_kernel = 1;
    l1_access.exclude_hv = 1;

    memset(&l1_miss, 0, sizeof(struct perf_event_attr));
    l1_miss.type = PERF_TYPE_HW_CACHE;
    l1_miss.size = sizeof(struct perf_event_attr);
    l1_miss.config =  ((PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16)) ;
    l1_miss.disabled = 1;
    l1_miss.exclude_kernel = 1;
    l1_miss.exclude_hv = 1;

    double time_used;
    // for(int l=256;l<=320;l+=16){
        int long long data_size = 128 * 1024;
        float* data = (float*)malloc(data_size);
        
        // memset(tail, 0, 4);

        int long long looptime=1000000;
        int inner_loop = data_size / 512;
        for (int i = 0; i < data_size/sizeof(float); i++) {
            data[i] = i;
            // data2[i] = i;
        }
        // load_ldp_kernel(data, inner_loop, looptime);
        if (start_perf_monitoring(&l1_access, &fd1) != 0) {
            exit(EXIT_FAILURE);
        }
        if (start_perf_monitoring(&l1_miss, &fd2) != 0) {
            exit(EXIT_FAILURE);
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);

        load_ldp_kernel(data, inner_loop, looptime);
        
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        ioctl(fd1, PERF_EVENT_IOC_DISABLE, 0);
        ioctl(fd2, PERF_EVENT_IOC_DISABLE, 0);
        read(fd1, &count1, sizeof(long long));
        read(fd2, &count2, sizeof(long long));
        printf("L1 Cache Load : %lld  L1 Cache Load Miss : %lld \n", count1,count2);
        printf("Total load = %lld Byte \n", looptime * data_size);
        close(fd1);
        close(fd2);


        time_used = get_time(&start, &end);
        free(data);
        double perf = looptime * data_size /
            (time_used * 2.6 * 1e9);
        printf("time = %.10f, perf = %.2f byte/cycle \n", time_used, perf);
        // printf("%d, perf = %.2f byte/cycle \n", l, perf);
    // }
    return 0;
}