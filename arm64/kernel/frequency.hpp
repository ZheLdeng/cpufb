#ifndef CPUFB_ARM64_FREQUENCY_HPP
#define CPUFB_ARM64_FREQUENCY_HPP
#include <string>
#include <vector>
#include "table.hpp"
struct FrequencyData
{
    double theory_freq = 0;
    // Measured clock shown as "Test Freq"; 0 when nothing could be measured.
    double caculate_freq = 0;
    // Clock used to normalize IPC; equals caculate_freq when measured,
    // otherwise a reported or user-supplied value.
    double clock_ghz = 0;
    std::string counter_source = "unavailable";
    double IPC_fp32 = 0;
    double IPC_fp64 = 0;
    double IPC_load = 0;
#ifdef _SVE_
    double IPC_fp32_sve = 0;
    double IPC_fp64_sve = 0;
#endif
};
void get_cpu_freq(std::vector<int> &set_of_threads, Table &table);
// Source of the clock in freq[0] ("perf_event cycles", "ADD-chain estimate",
// ...), for tables that fall back to it.
const std::string &cpu_freq_counter_source();

#endif
