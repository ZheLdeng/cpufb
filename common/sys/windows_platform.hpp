#ifndef CPUFB_WINDOWS_PLATFORM_HPP
#define CPUFB_WINDOWS_PLATFORM_HPP

#ifdef _WIN32

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cpufb {
namespace windows {

struct CacheInfo
{
    std::uint64_t bytes;
    int level;
    int line_bytes;
    int ways;
    int shared_cpus;
};

int current_processor();
bool pin_current_thread(int cpu);
std::size_t page_size();
std::uint64_t available_memory_bytes();
std::string os_version();
std::string cpu_model();
double reported_base_frequency_ghz();
double reported_max_frequency_ghz();
double reported_current_frequency_ghz(int cpu, std::string &source);
std::string timestamp();
std::vector<CacheInfo> data_caches(int cpu);

} // namespace windows
} // namespace cpufb

#endif // _WIN32

#endif // CPUFB_WINDOWS_PLATFORM_HPP
