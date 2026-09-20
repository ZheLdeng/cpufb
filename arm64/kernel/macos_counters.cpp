#include "macos_counters.hpp"

#include <dlfcn.h>

#include <cstdio>
#include <cstring>
#include <regex>

namespace {
constexpr uint32_t kKpcClassFixedMask = 1u;
constexpr uint32_t kFixedCounterCount = 2;
} // namespace

MacosCounters::MacosCounters()
{
    library_ = dlopen(
        "/System/Library/PrivateFrameworks/kperf.framework/kperf", RTLD_LAZY);
    if (library_ == nullptr) {
        error_ = "kperf framework unavailable";
        return;
    }

    get_thread_counters_ = reinterpret_cast<int (*)(uint32_t *, uint64_t *)>(
        dlsym(library_, "kpc_get_curthread_counters"));
    get_counter_count_ = reinterpret_cast<uint32_t (*)(uint32_t)>(
        dlsym(library_, "kpc_get_counter_count"));
    if (get_thread_counters_ == nullptr || get_counter_count_ == nullptr) {
        error_ = "kperf fixed-counter API unavailable";
        dlclose(library_);
        library_ = nullptr;
    }
}

MacosCounters::~MacosCounters()
{
    if (library_ != nullptr) dlclose(library_);
}

bool MacosCounters::read(MacosCounterSnapshot &snapshot) const
{
    if (!available()) return false; // error_ was set by the constructor
    if (get_counter_count_(kKpcClassFixedMask) < kFixedCounterCount) {
        error_ = "kperf reports fewer than two fixed counters";
        return false;
    }

    uint32_t count = kFixedCounterCount;
    uint64_t values[kFixedCounterCount] = {};
    if (get_thread_counters_(&count, values) != 0 ||
        count < kFixedCounterCount) {
        // The usual case without root: thread counting is not enabled.
        error_ = "kpc_get_curthread_counters denied (needs root)";
        return false;
    }
    error_.clear();

    // Apple KPC's fixed-counter order is cycles followed by instructions.
    snapshot.cycles = values[0];
    snapshot.instructions = values[1];
    return true;
}

bool MacosCounters::available() const
{
    return library_ != nullptr && get_thread_counters_ != nullptr &&
        get_counter_count_ != nullptr;
}

std::string MacosCounters::error() const
{
    return error_;
}

bool sample_powermetrics_frequency_mhz(
    double &frequency_mhz, std::string &error)
{
    FILE *pipe =
        popen("/usr/bin/powermetrics -n 1 -i 20 -s cpu_power 2>/dev/null", "r");
    if (pipe == nullptr) {
        error = "unable to launch powermetrics";
        return false;
    }

    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
    int result = pclose(pipe);
    if (result != 0) {
        error = "powermetrics requires root privileges";
        return false;
    }

    const std::regex frequency_pattern(
        "(?:P-Cluster|P-core|CPU).*?[Ff]requency[^0-9]*([0-9]+(?:\\.[0-9]+)?)\\s*MHz");
    std::smatch match;
    if (!std::regex_search(output, match, frequency_pattern)) {
        error = "powermetrics did not report a P-core frequency";
        return false;
    }
    frequency_mhz = std::stod(match[1].str());
    return frequency_mhz > 0;
}
