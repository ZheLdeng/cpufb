#include "cache_topology.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace cpufb {
namespace {

constexpr std::uint64_t kKiB = 1024;
constexpr std::uint64_t kMiB = kKiB * kKiB;
constexpr std::uint64_t kGiB = kMiB * kKiB;
constexpr std::uint64_t kMinimumStreamWorkset = 256 * kMiB;
constexpr std::uint64_t kCacheMultiplier = 4;

std::string trim(std::string value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lowercase(std::string value)
{
    for (char &ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

bool read_text_file(const std::string &path, std::string &value)
{
    std::ifstream input(path.c_str());
    if (!input) return false;
    std::getline(input, value);
    value = trim(value);
    return !value.empty();
}

bool parse_positive_integer(const std::string &text, std::uint64_t &value)
{
    const std::string cleaned = trim(text);
    if (cleaned.empty()) return false;

    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(cleaned.c_str(), &end, 10);
    if (errno != 0 || end == cleaned.c_str() || parsed == 0) return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;

    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_cache_size(const std::string &text, std::uint64_t &value)
{
    const std::string cleaned = trim(text);
    if (cleaned.empty()) return false;

    errno = 0;
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(cleaned.c_str(), &end, 10);
    if (errno != 0 || end == cleaned.c_str() || parsed == 0) return false;

    std::uint64_t multiplier = 1;
    if (*end != '\0') {
        switch (std::tolower(static_cast<unsigned char>(*end))) {
        case 'k': multiplier = kKiB; break;
        case 'm': multiplier = kMiB; break;
        case 'g': multiplier = kGiB; break;
        default: return false;
        }
        ++end;
        if (std::tolower(static_cast<unsigned char>(*end)) == 'b') ++end;
    }
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0' ||
        static_cast<std::uint64_t>(parsed) >
            std::numeric_limits<std::uint64_t>::max() / multiplier)
        return false;

    value = static_cast<std::uint64_t>(parsed) * multiplier;
    return true;
}

void consider_cache(LastLevelCacheInfo &best,
    std::uint64_t bytes,
    int level,
    bool is_reported_llc,
    const std::string &source)
{
    if (bytes == 0 || level < best.level ||
        (level == best.level && bytes <= best.bytes))
        return;
    best.bytes = bytes;
    best.level = level;
    best.is_reported_llc = is_reported_llc;
    best.source = source;
}

void consider_cache_level(CacheLevelInfo &best,
    std::uint64_t bytes,
    int level,
    const std::string &source)
{
    if (bytes == 0 || bytes <= best.bytes) return;
    best.bytes = bytes;
    best.level = level;
    best.source = source;
}

#ifdef __linux__
struct LinuxCacheEntry
{
    std::uint64_t bytes;
    int level;
    std::string source;
};

std::vector<LinuxCacheEntry> read_linux_data_caches(int cpu)
{
    std::vector<LinuxCacheEntry> entries;
    if (cpu < 0) return entries;

    const std::string cache_root = "/sys/devices/system/cpu/cpu" +
        std::to_string(cpu) + "/cache/";
    // Linux exposes cache entries as indexN.  The index is not guaranteed to
    // correspond to the cache level, so inspect every reasonable entry.
    for (int index = 0; index < 32; ++index) {
        const std::string root = cache_root + "index" + std::to_string(index) + "/";
        std::string type;
        std::string level_text;
        std::string size_text;
        if (!read_text_file(root + "type", type) ||
            !read_text_file(root + "level", level_text) ||
            !read_text_file(root + "size", size_text))
            continue;

        const std::string normalized_type = lowercase(type);
        if (normalized_type != "data" && normalized_type != "unified")
            continue;

        std::uint64_t level = 0;
        std::uint64_t bytes = 0;
        if (!parse_positive_integer(level_text, level) ||
            level > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
            !parse_cache_size(size_text, bytes))
            continue;

        const int cache_level = static_cast<int>(level);
        entries.push_back({bytes, cache_level,
            "Linux sysfs cpu" + std::to_string(cpu) + "/cache/index" +
            std::to_string(index) + " (L" + std::to_string(cache_level) +
            " " + type + ")"});
    }
    return entries;
}

CacheLevelInfo detect_linux_data_cache_level(int cpu, int level)
{
    CacheLevelInfo best;
    if (level <= 0) return best;
    for (const LinuxCacheEntry &entry : read_linux_data_caches(cpu)) {
        if (entry.level == level)
            consider_cache_level(best, entry.bytes, entry.level, entry.source);
    }
    return best;
}

LastLevelCacheInfo detect_linux_last_level_cache(int cpu)
{
    LastLevelCacheInfo best;
    for (const LinuxCacheEntry &entry : read_linux_data_caches(cpu)) {
        consider_cache(best, entry.bytes, entry.level, true, entry.source);
    }
    return best;
}
#endif

#ifdef __APPLE__
bool read_sysctl_u64(const std::string &name, std::uint64_t &value)
{
    std::uint64_t result = 0;
    std::size_t size = sizeof(result);
    // Cache-size sysctls are exposed as either 32-bit or 64-bit integers
    // across macOS releases.  A zero-initialized u64 safely receives both.
    if (sysctlbyname(name.c_str(), &result, &size, nullptr, 0) != 0 ||
        size == 0 || size > sizeof(result) || result == 0)
        return false;
    value = result;
    return true;
}

void consider_macos_sysctl(LastLevelCacheInfo &best,
    const std::string &name,
    int level,
    bool is_reported_llc)
{
    std::uint64_t bytes = 0;
    if (read_sysctl_u64(name, bytes))
        consider_cache(best, bytes, level, is_reported_llc, "macOS " + name);
}

LastLevelCacheInfo detect_macos_last_level_cache()
{
    LastLevelCacheInfo l3;
    consider_macos_sysctl(l3, "hw.l3cachesize", 3, true);
    for (int perf_level = 0; perf_level < 16; ++perf_level) {
        consider_macos_sysctl(l3,
            "hw.perflevel" + std::to_string(perf_level) + ".l3cachesize",
            3, true);
    }
    if (l3.bytes != 0) return l3;

    LastLevelCacheInfo l2;
    consider_macos_sysctl(l2, "hw.l2cachesize", 2, false);
    for (int perf_level = 0; perf_level < 16; ++perf_level) {
        consider_macos_sysctl(l2,
            "hw.perflevel" + std::to_string(perf_level) + ".l2cachesize",
            2, false);
    }
    if (l2.bytes != 0)
        l2.source += " (outermost cache reported by macOS)";
    return l2;
}

CacheLevelInfo detect_macos_data_cache_level(int level)
{
    CacheLevelInfo best;
    if (level < 1 || level > 3) return best;

    const std::string suffix = level == 1 ? "l1dcachesize" :
        (level == 2 ? "l2cachesize" : "l3cachesize");
    for (int perf_level = 0; perf_level < 16; ++perf_level) {
        const std::string name = "hw.perflevel" +
            std::to_string(perf_level) + "." + suffix;
        std::uint64_t bytes = 0;
        if (read_sysctl_u64(name, bytes))
            consider_cache_level(best, bytes, level, "macOS " + name);
    }
    if (best.bytes != 0) return best;

    const std::string name = "hw." + suffix;
    std::uint64_t bytes = 0;
    if (read_sysctl_u64(name, bytes))
        consider_cache_level(best, bytes, level, "macOS " + name);
    return best;
}
#endif

} // namespace

CacheLevelInfo detect_data_cache_level(int cpu, int level)
{
#ifdef __linux__
    return detect_linux_data_cache_level(cpu, level);
#elif defined(__APPLE__)
    (void)cpu;
    return detect_macos_data_cache_level(level);
#else
    (void)cpu;
    (void)level;
    return CacheLevelInfo();
#endif
}

LastLevelCacheInfo detect_last_level_cache(int cpu)
{
#ifdef __linux__
    return detect_linux_last_level_cache(cpu);
#elif defined(__APPLE__)
    (void)cpu;
    return detect_macos_last_level_cache();
#else
    (void)cpu;
    return LastLevelCacheInfo();
#endif
}

std::uint64_t recommended_stream_workset_bytes(const LastLevelCacheInfo &cache)
{
    std::uint64_t recommended = kMinimumStreamWorkset;
    if (cache.bytes == 0) return recommended;
    if (cache.bytes > std::numeric_limits<std::uint64_t>::max() / kCacheMultiplier)
        return std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t cache_scaled = cache.bytes * kCacheMultiplier;
    return cache_scaled > recommended ? cache_scaled : recommended;
}

std::string format_cache_capacity(std::uint64_t bytes)
{
    if (bytes == 0) return "0 B";
    if (bytes % kGiB == 0) return std::to_string(bytes / kGiB) + " GiB";
    if (bytes % kMiB == 0) return std::to_string(bytes / kMiB) + " MiB";
    if (bytes % kKiB == 0) return std::to_string(bytes / kKiB) + " KiB";
    return std::to_string(bytes) + " B";
}

} // namespace cpufb
