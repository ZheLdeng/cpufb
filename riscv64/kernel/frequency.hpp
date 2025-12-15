#ifndef _FREQUENCY_HPP
#define _FREQUENCY_HPP
#include <string>  
#include <vector>
#include "table.hpp"
struct FrequencyData {
    double theory_freq = 0;
    double caculate_freq = 0;
    double IPC_fp32 = 0;
    double IPC_fp64 = 0;
    double IPC_load = 0;
    #ifdef _SVE_
    double IPC_fp32_sve = 0;
    double IPC_fp64_sve = 0;
    #endif
};
void get_cpu_freq(std::vector<int> &set_of_threads,Table &table);

#endif
