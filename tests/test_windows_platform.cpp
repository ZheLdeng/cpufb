#include "sys/system_info.hpp"
#include "sys/windows_platform.hpp"

#include <iostream>
#include <regex>
#include <string>

int main()
{
    const std::string timestamp = cpufb::windows::timestamp();
    if (!std::regex_match(timestamp,
            std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:"
                       "[0-9]{2}:[0-9]{2}[+-][0-9]{4}"))) {
        std::cerr << "Invalid numeric-offset timestamp: " << timestamp << '\n';
        return 1;
    }
    std::string source;
    if (cpufb::windows::reported_current_frequency_ghz(-1, source) != 0.0 ||
        source.empty()) {
        std::cerr << "Unavailable frequency must include a reason\n";
        return 1;
    }
    bool have_timestamp = false;
    bool have_frequency = false;
    const cpufb::SystemInfo info = cpufb::collect_system_info({0});
    for (const cpufb::SystemInfoEntry &entry : info.entries) {
        if (entry.item == "Sample Timestamp")
            have_timestamp = std::regex_match(entry.value,
                std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:"
                           "[0-9]{2}:[0-9]{2}[+-][0-9]{4}"));
        if (entry.item != "CPU Frequency") continue;
        have_frequency = !entry.source.empty();
        if (cpufb::windows::reported_base_frequency_ghz() > 0.0 &&
            entry.value.find("base ") == std::string::npos) {
            std::cerr << "CPUID base frequency missing from system output\n";
            return 1;
        }
        if (cpufb::windows::reported_max_frequency_ghz() > 0.0 &&
            entry.value.find("max/turbo ") == std::string::npos) {
            std::cerr << "CPUID maximum frequency missing from system output\n";
            return 1;
        }
    }
    if (!have_timestamp || !have_frequency) {
        std::cerr << "System information output is incomplete\n";
        return 1;
    }
    return 0;
}
