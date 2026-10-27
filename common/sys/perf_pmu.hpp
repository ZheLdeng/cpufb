#ifndef CPUFB_PERF_PMU_HPP
#define CPUFB_PERF_PMU_HPP

// perf_event configuration for the CPU the calling thread runs on.
//
// A heterogeneous machine (Arm big.LITTLE, Intel hybrid) has one PMU per core
// type, each a separate perf event source.  A plain PERF_TYPE_HARDWARE cycles
// event binds to the first of them, so on every other core type it opens
// without error and counts nothing; on a MediaTek MT6993 only cpu0-3 ever
// produced cycle counts.  Since Linux 5.x the PMU can be named in the upper
// 32 bits of attr.config of a PERF_TYPE_HARDWARE event; the PMUs and the CPUs
// they cover are listed under /sys/bus/event_source/devices/*/cpus.

#ifdef __linux__

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <linux/perf_event.h>
#include <sched.h>
#include <string>

namespace cpufb {

// True when the range list ("0-3,7") contains cpu.
inline bool cpu_list_contains(const std::string &list, int cpu)
{
    const char *cursor = list.c_str();
    while (*cursor != '\0') {
        char *end = nullptr;
        const long first = std::strtol(cursor, &end, 10);
        if (end == cursor) break;
        long last = first;
        if (*end == '-') {
            const char *range = end + 1;
            last = std::strtol(range, &end, 10);
            if (end == range) break;
        }
        if (cpu >= first && cpu <= last) return true;
        cursor = *end == ',' ? end + 1 : end;
    }
    return false;
}

// attr.config for PERF_COUNT_HW_CPU_CYCLES on the current CPU: the plain
// event on a machine with one CPU PMU, the PMU-qualified one otherwise.
inline std::uint64_t perf_cycles_config_for_current_cpu()
{
    const int cpu = sched_getcpu();
    std::uint64_t config = PERF_COUNT_HW_CPU_CYCLES;
    DIR *devices = opendir("/sys/bus/event_source/devices");
    if (cpu < 0 || devices == nullptr) return config;

    int cpu_pmus = 0;
    std::uint64_t matching_type = 0;
    while (dirent *entry = readdir(devices)) {
        if (entry->d_name[0] == '.') continue;
        const std::string base =
            std::string("/sys/bus/event_source/devices/") + entry->d_name;
        std::string cpus;
        std::ifstream cpus_file((base + "/cpus").c_str());
        if (!cpus_file || !std::getline(cpus_file, cpus) || cpus.empty())
            continue; // software, tracepoint, uncore: no CPU list
        ++cpu_pmus;
        std::ifstream type_file((base + "/type").c_str());
        unsigned long type = 0;
        if (type_file >> type && cpu_list_contains(cpus, cpu))
            matching_type = type;
    }
    closedir(devices);
    // With a single CPU PMU the plain event already binds to it, and older
    // kernels reject the qualified form.
    if (cpu_pmus > 1 && matching_type != 0) config |= matching_type << 32;
    return config;
}

} // namespace cpufb

#endif // __linux__

#endif // CPUFB_PERF_PMU_HPP
