#ifndef CPUFB_X64_FREQUENCY_HPP
#define CPUFB_X64_FREQUENCY_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "table.hpp"

extern "C"
{
    void cpufb_x64_frequency_fsu32(int64_t loop_time);
    void cpufb_x64_frequency_fsu64(int64_t loop_time);
    void cpufb_x64_frequency_load(const void *data, int64_t loop_time);
    uint64_t cpufb_x64_frequency_add_chain(int64_t loop_time, uint64_t addend);
}

struct FrequencyData {
    double theory_freq = 0;
    // Measured clock shown as "Test Freq"; 0 when nothing could be measured.
    double caculate_freq = 0;
    // Clock used to normalize IPC: caculate_freq, or a supplied/reported one.
    double clock_ghz = 0;
    double tsc_freq = 0;
    std::string counter_source = "unavailable";
    double IPC_fp32 = 0;
    double IPC_fp64 = 0;
    double IPC_load = 0;
};
void get_cpu_freq(std::vector<int> &set_of_threads,Table &table);
// Source of the cycle rate stored in freq[0]: "perf_event cycles",
// "CPUFB_FREQ_GHZ (not measured)", "ADD-chain estimate", "invariant TSC" or
// "OS-reported frequency (not measured)".
const std::string &cpu_freq_counter_source();

#endif
