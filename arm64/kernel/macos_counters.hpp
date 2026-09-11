#ifndef _MACOS_COUNTERS_HPP
#define _MACOS_COUNTERS_HPP

#include <cstdint>
#include <string>

struct MacosCounterSnapshot {
    uint64_t cycles = 0;
    uint64_t instructions = 0;
};

class MacosCounters {
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
    std::string error_;
};

// powermetrics is a root-only, sampled fallback. The returned value is MHz.
bool sample_powermetrics_frequency_mhz(double &frequency_mhz, std::string &error);

#endif