#ifndef CPUFB_SYSTEM_INFO_HPP
#define CPUFB_SYSTEM_INFO_HPP

#include "table.hpp"

#include <string>
#include <vector>

namespace cpufb {

struct SystemInfoEntry
{
    std::string item;
    std::string value;
    std::string source;
};

struct SystemInfo
{
    std::vector<SystemInfoEntry> entries;
};

SystemInfo collect_system_info(const std::vector<int> &selected_cores);
void populate_system_info_table(const SystemInfo &info, Table &table);

#ifdef __APPLE__
// Highest CPU DVFS frequency published by the Apple Silicon power manager in
// the IORegistry (pmgr "voltage-states*-sram"), in GHz, or 0 when unavailable.
// This is the OS-reported nominal maximum, not a measurement; hw.cpufrequency
// does not exist on Apple Silicon.
double macos_reported_max_frequency_ghz();
#endif

} // namespace cpufb

#endif