#ifndef _FREQUENCY_HPP
#define _FREQUENCY_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "table.hpp"

extern "C"
{
    void cpufb_x64_frequency_fsu32(int64_t loop_time);
    void cpufb_x64_frequency_fsu64(int64_t loop_time);
    void cpufb_x64_frequency_load(const void *data, int64_t loop_time);
}

struct FrequencyData {
    double theory_freq = 0;
    double caculate_freq = 0;
    double IPC_fp32 = 0;
    double IPC_fp64 = 0;
    double IPC_load = 0;
    // #ifdef _SVE_FMLA_
    // double IPC_fp32_sve = 0;
    // double IPC_fp64_sve = 0;
    // #endif
};
void get_cpu_freq(std::vector<int> &set_of_threads,Table &table);

#endif
