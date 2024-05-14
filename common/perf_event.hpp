#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

class PerfEventCycle {
private:
    int fd;
    struct perf_event_attr pe;
    long long count;

public:
    PerfEventCycle() {
        // 初始化性能事件属性结构
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = PERF_COUNT_HW_CPU_CYCLES; 
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        // 打开性能计数器文件描述符
        fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
        if (fd == -1) {
            perror("perf_event_open");
            exit(EXIT_FAILURE);
        }
    }

    void start() {
        // 启用性能计数器
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    void stop() {
        // 禁用性能计数器
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

        // 读取 CPU 周期数
        read(fd, &count, sizeof(long long));

        close(fd);
    }
    long long get_cycle(){
        return count;
    }
};

