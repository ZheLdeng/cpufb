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

} // namespace cpufb

#endif