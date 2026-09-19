#ifndef CPUFB_COMMON_HPP
#define CPUFB_COMMON_HPP

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstdint>
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
    PerfEventCycle(int mode = 0, bool report_errors = true) : fd(-1), count(0) {
        // Initialise the perf_event attribute structure
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(struct perf_event_attr);
        if (mode == 0) {
            pe.config = PERF_COUNT_HW_CPU_CYCLES;
        } else {
            pe.config = 0xA00000000;
        }
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;

        // Open the performance-counter file descriptor
        fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
        if (fd == -1 && report_errors) {
            perror("perf_event_open");
            std::cerr << "Warning: hardware cycle counter unavailable; "
                         "falling back to the reported CPU frequency"
                      << std::endl;
        }
    }

    ~PerfEventCycle() {
        if (fd != -1) close(fd);
    }

    // Owns a file descriptor: a copy would close it twice.
    PerfEventCycle(const PerfEventCycle &) = delete;
    PerfEventCycle &operator=(const PerfEventCycle &) = delete;

    bool available() const {
        return fd != -1;
    }

    void start() {
        if (fd == -1) return;
        // Reset and enable the counter
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    void stop() {
        if (fd == -1) {
            count = 0;
            return;
        }
        // Disable the counter
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

        // Read the CPU cycle count
        const ssize_t bytes_read = read(fd, &count, sizeof(count));
        if (bytes_read != static_cast<ssize_t>(sizeof(count))) {
            count = 0;
        }

        close(fd);
        fd = -1;
    }
    long long get_cycle(){
        return count;
    }
};

inline void read_data(int cpu_id, int *data, std::string path)
{
    FILE *fp = nullptr;
    char buf[100] = {0};
    std::string file_path="/sys/devices/system/cpu/cpu"+ std::to_string(cpu_id) + path;
    std::ifstream file(file_path);
    if (file) {
        std::string read_freq = "cat " + file_path;
        fp = popen(read_freq.c_str(), "r");
        if (fp) {
            int ret = fread(buf, 1, sizeof(buf)-1, fp);
            if (ret > 0) {
                *data = std::stod(buf);
            }
            pclose(fp);
        }
    }
}
#endif

#ifdef _SME_
extern "C" uint64_t load_sme_vector_bytes(void);

static uint64_t rdsvl()
{
  return load_sme_vector_bytes();
}
#endif

#endif
