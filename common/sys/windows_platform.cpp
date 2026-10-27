#ifdef _WIN32

#include "sys/windows_platform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <powrprof.h>
#include <cpuid.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace cpufb {
namespace windows {
namespace {

struct ProcessorLocation
{
    WORD group;
    BYTE number;
};

// MinGW's user-mode headers omit PROCESSOR_POWER_INFORMATION. This is
// the documented six-ULONG output layout for ProcessorInformation.
struct ProcessorPowerInformation
{
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
};

std::vector<WORD> processor_group_sizes()
{
    const WORD group_count = GetActiveProcessorGroupCount();
    std::vector<WORD> sizes(group_count);
    for (WORD group = 0; group < group_count; ++group)
        sizes[group] = GetActiveProcessorCount(group);
    return sizes;
}

bool processor_location(int cpu, ProcessorLocation &location)
{
    if (cpu < 0) return false;
    const std::vector<WORD> sizes = processor_group_sizes();
    unsigned remaining = static_cast<unsigned>(cpu);
    for (WORD group = 0; group < sizes.size(); ++group) {
        if (remaining < sizes[group]) {
            location.group = group;
            location.number = static_cast<BYTE>(remaining);
            return true;
        }
        remaining -= sizes[group];
    }
    return false;
}

int flatten_processor(WORD group, DWORD number)
{
    const std::vector<WORD> sizes = processor_group_sizes();
    if (group >= sizes.size() || number >= sizes[group]) return -1;
    int cpu = static_cast<int>(number);
    for (WORD previous = 0; previous < group; ++previous)
        cpu += sizes[previous];
    return cpu;
}

bool cache_contains_processor(
    const CACHE_RELATIONSHIP &cache, const ProcessorLocation &processor)
{
    for (WORD index = 0; index < cache.GroupCount; ++index) {
        const GROUP_AFFINITY &group = cache.GroupMasks[index];
        if (group.Group == processor.group &&
            (group.Mask & (static_cast<KAFFINITY>(1) << processor.number)))
            return true;
    }
    return false;
}

int cache_shared_processors(const CACHE_RELATIONSHIP &cache)
{
    int count = 0;
    for (WORD index = 0; index < cache.GroupCount; ++index)
        count += static_cast<int>(
            __builtin_popcountll(static_cast<unsigned long long>(
                cache.GroupMasks[index].Mask)));
    return count;
}

} // namespace

int current_processor()
{
    PROCESSOR_NUMBER number = {};
    GetCurrentProcessorNumberEx(&number);
    return flatten_processor(number.Group, number.Number);
}

bool pin_current_thread(int cpu)
{
    ProcessorLocation location = {};
    if (!processor_location(cpu, location)) return false;
    GROUP_AFFINITY target = {};
    target.Group = location.group;
    target.Mask = static_cast<KAFFINITY>(1) << location.number;
    return SetThreadGroupAffinity(GetCurrentThread(), &target, nullptr) != 0;
}

std::size_t page_size()
{
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    return info.dwPageSize;
}

std::uint64_t available_memory_bytes()
{
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullAvailPhys : 0;
}

std::string os_version()
{
    typedef LONG(WINAPI *RtlGetVersionFn)(OSVERSIONINFOW *);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const RtlGetVersionFn get_version = ntdll == nullptr
        ? nullptr
        : reinterpret_cast<RtlGetVersionFn>(
              GetProcAddress(ntdll, "RtlGetVersion"));
    if (get_version == nullptr) return "Windows";
    OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    if (get_version(&version) != 0) return "Windows";
    std::ostringstream result;
    result << "Windows " << version.dwMajorVersion << '.'
           << version.dwMinorVersion << " (build " << version.dwBuildNumber
           << ')';
    return result.str();
}

std::string cpu_model()
{
    unsigned max_leaf = __get_cpuid_max(0x80000000, nullptr);
    if (max_leaf < 0x80000004)
        return "";
    char brand[49] = {};
    for (int leaf = 0; leaf < 3; ++leaf) {
        unsigned eax, ebx, ecx, edx;
        __cpuid(0x80000002 + leaf, eax, ebx, ecx, edx);
        const unsigned registers[] = {eax, ebx, ecx, edx};
        std::copy(reinterpret_cast<const char *>(registers),
            reinterpret_cast<const char *>(registers) + sizeof(registers),
            brand + leaf * 16);
    }
    std::string result(brand);
    const std::size_t first = result.find_first_not_of(" \t");
    const std::size_t last = result.find_last_not_of(" \t");
    if (first == std::string::npos) return "";
    return result.substr(first, last - first + 1);
}

double reported_base_frequency_ghz()
{
    unsigned eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, nullptr) < 0x16) return 0.0;
    __cpuid(0x16, eax, ebx, ecx, edx);
    return static_cast<double>(eax & 0xffff) * 1e-3;
}

double reported_max_frequency_ghz()
{
    unsigned eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, nullptr) < 0x16) return 0.0;
    __cpuid(0x16, eax, ebx, ecx, edx);
    return static_cast<double>(ebx & 0xffff) * 1e-3;
}

double reported_current_frequency_ghz(int cpu, std::string &source)
{
    // ProcessorInformation has no processor-group selector. Do not attach
    // a frequency from the wrong group to a selected CPU on large systems.
    if (GetActiveProcessorGroupCount() != 1) {
        source = "current frequency unavailable on multiple processor groups";
        return 0.0;
    }
    const DWORD count = GetActiveProcessorCount(0);
    if (cpu < 0 || static_cast<DWORD>(cpu) >= count) {
        source = "current frequency unavailable: invalid CPU selection";
        return 0.0;
    }
    std::vector<ProcessorPowerInformation> processors(count);
    const ULONG status = CallNtPowerInformation(ProcessorInformation,
        nullptr, 0, processors.data(),
        static_cast<ULONG>(processors.size() * sizeof(processors[0])));
    if (status != 0) {
        source = "CallNtPowerInformation failed (status " +
            std::to_string(status) + ")";
        return 0.0;
    }
    for (const ProcessorPowerInformation &processor : processors) {
        if (processor.Number == static_cast<ULONG>(cpu) &&
            processor.CurrentMhz > 0) {
            source = "CallNtPowerInformation (OS-reported current)";
            return static_cast<double>(processor.CurrentMhz) * 1e-3;
        }
    }
    source = "CallNtPowerInformation: current frequency not reported";
    return 0.0;
}

std::string timestamp()
{
    SYSTEMTIME local = {};
    GetLocalTime(&local);
    TIME_ZONE_INFORMATION zone = {};
    const DWORD state = GetTimeZoneInformation(&zone);
    std::ostringstream result;
    result << std::setfill('0') << std::setw(4) << local.wYear << '-'
           << std::setw(2) << local.wMonth << '-' << std::setw(2)
           << local.wDay << 'T' << std::setw(2) << local.wHour << ':'
           << std::setw(2) << local.wMinute << ':' << std::setw(2)
           << local.wSecond;
    if (state == TIME_ZONE_ID_INVALID) {
        result << " (timezone unavailable; Win32 error " << GetLastError()
               << ')';
        return result.str();
    }
    LONG bias = zone.Bias;
    if (state == TIME_ZONE_ID_DAYLIGHT) bias += zone.DaylightBias;
    if (state == TIME_ZONE_ID_STANDARD) bias += zone.StandardBias;
    const LONG offset = -bias;
    const LONG minutes = offset < 0 ? -offset : offset;
    result << (offset < 0 ? '-' : '+') << std::setw(2) << minutes / 60
           << std::setw(2) << minutes % 60;
    return result.str();
}

std::vector<CacheInfo> data_caches(int cpu)
{
    std::vector<CacheInfo> result;
    ProcessorLocation processor = {};
    if (!processor_location(cpu, processor)) return result;

    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationCache, nullptr, &bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0) return result;
    std::vector<unsigned char> storage(bytes);
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX record =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
            storage.data());
    if (!GetLogicalProcessorInformationEx(RelationCache, record, &bytes))
        return result;

    for (DWORD offset = 0; offset < bytes;) {
        record = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
            storage.data() + offset);
        if (record->Size == 0 || record->Size > bytes - offset) break;
        if (record->Relationship == RelationCache &&
            (record->Cache.Type == CacheData ||
                record->Cache.Type == CacheUnified) &&
            record->Cache.Level <= 3 &&
            cache_contains_processor(record->Cache, processor)) {
            CacheInfo info = {record->Cache.CacheSize,
                static_cast<int>(record->Cache.Level),
                static_cast<int>(record->Cache.LineSize),
                record->Cache.Associativity == 0xff
                    ? 0
                    : static_cast<int>(record->Cache.Associativity),
                cache_shared_processors(record->Cache)};
            result.push_back(info);
        }
        offset += record->Size;
    }
    return result;
}

} // namespace windows
} // namespace cpufb

#endif // _WIN32
