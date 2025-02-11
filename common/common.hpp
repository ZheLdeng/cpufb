#ifndef _PERF_EVENT_HPP
#define _PERF_EVENT_HPP

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/ioctl.h>

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}
#ifdef __linux__
#include <asm/unistd.h>
#include <linux/perf_event.h>
class PerfEventCycle {
private:
    int fd;
    struct perf_event_attr pe;
    long long count;
    public:
    PerfEventCycle(int mode = 0) {
        // 初始化性能事件属性结构
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(struct perf_event_attr);
        if (mode == 0) {
            pe.config = PERF_COUNT_HW_CPU_CYCLES;
        } else {
            pe.config = 0xA00000000;
        }
        // pe.config = PERF_COUNT_HW_CPU_CYCLES;
        // pe.config = 0xA00000000;
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

inline void read_data(int cpu_id, int *data, std::string path)
{
    FILE *fp = NULL;
    char buf[100] = {0};
    std::string file_path="/sys/devices/system/cpu/cpu"+ std::to_string(cpu_id) + path;
    std::ifstream file(file_path);
    if (file) {
        std::string read_freq = "cat " + file_path;
        fp = popen(read_freq.c_str(), "r");
        if (fp) {
            int ret = fread(buf, 1, sizeof(buf)-1, fp);
            if (ret > 0) {
                // data->theory_freq = std::stod(buf) * 1e-6;
                *data = std::stod(buf);
            }
            pclose(fp);
        }
    }
}
#endif

#ifdef _SME_
static uint64_t rdsvl8()
{
  uint64_t len;
  asm volatile ("rdsvl %0, 8"
                : "=r"(len)
               );
  return len;
}
#endif

#endif

