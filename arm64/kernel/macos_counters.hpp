#ifndef CPUFB_ARM64_MACOS_COUNTERS_HPP
#define CPUFB_ARM64_MACOS_COUNTERS_HPP

#include <cstdint>
#include <string>

struct MacosCounterSnapshot
{
    uint64_t cycles = 0;
    uint64_t instructions = 0;
};

class MacosCounters
{
public:
    MacosCounters();
    ~MacosCounters();

    bool read(MacosCounterSnapshot &snapshot) const;
    bool available() const;
    std::string error() const;

private:
    void *library_ = nullptr;
    int (*get_thread_counters_)(uint32_t *, uint64_t *) = nullptr;
    uint32_t (*get_counter_count_)(uint32_t) = nullptr;
    // Reason for the most recent failure; read() is logically const.
    mutable std::string error_;
};

// Highest CPU DVFS frequency published by the power manager in the
// IORegistry (pmgr "voltage-states*-sram"), in GHz, or 0 when unavailable.
// This is the OS-reported nominal maximum, not a measurement.
double macos_reported_max_frequency_ghz();

// powermetrics is a root-only, sampled fallback. The returned value is MHz.
bool sample_powermetrics_frequency_mhz(
    double &frequency_mhz, std::string &error);

#endif