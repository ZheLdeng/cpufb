#include "system_info.hpp"

#include "cache_topology.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/utsname.h>
#include <vector>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace cpufb {
namespace {

std::string trim(std::string value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool read_first_line(const std::string &path, std::string &value)
{
    std::ifstream input(path.c_str());
    if (!input || !std::getline(input, value)) return false;
    value = trim(value);
    return !value.empty();
}

bool read_positive_number(const std::string &path, double &value)
{
    std::ifstream input(path.c_str());
    double result = 0.0;
    if (!(input >> result) || result <= 0.0) return false;
    value = result;
    return true;
}

std::string compiler_name()
{
#if defined(__clang__)
    return std::string("Clang ") + std::to_string(__clang_major__) + "." +
        std::to_string(__clang_minor__) + "." +
        std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return std::string("GCC ") + __VERSION__;
#elif defined(_MSC_VER)
    return std::string("MSVC ") + std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
}

std::string format_cores(const std::vector<int> &cores)
{
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < cores.size(); ++index) {
        if (index != 0) output << ',';
        output << cores[index];
    }
    output << ']';
    return output.str();
}

std::string format_ghz(double hz)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << hz * 1e-9 << " GHz";
    return output.str();
}

std::string current_timestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm local = {};
    localtime_r(&now, &local);
    char value[32] = {0};
    std::strftime(value, sizeof(value), "%Y-%m-%dT%H:%M:%S%z", &local);
    return value;
}

std::string read_temperature(std::string &source)
{
#ifdef __linux__
    double hottest_celsius = 0.0;
    std::string hottest_source;
    for (int zone = 0; zone < 128; ++zone) {
        const std::string root = "/sys/class/thermal/thermal_zone" +
            std::to_string(zone) + "/";
        double raw = 0.0;
        if (!read_positive_number(root + "temp", raw)) continue;
        const double celsius = raw > 1000.0 ? raw / 1000.0 : raw;
        if (celsius < 0.0 || celsius > 150.0 || celsius <= hottest_celsius)
            continue;
        hottest_celsius = celsius;
        std::string type;
        read_first_line(root + "type", type);
        hottest_source = root + "temp";
        if (!type.empty()) hottest_source += " (" + type + ")";
    }
    if (hottest_celsius > 0.0) {
        std::ostringstream output;
        output << std::fixed << std::setprecision(1) << hottest_celsius
               << " C";
        source = hottest_source;
        return output.str();
    }
#endif
    source = "no unprivileged CPU temperature interface";
    return "unavailable";
}

void add_entry(SystemInfo &info,
    const std::string &item,
    const std::string &value,
    const std::string &source)
{
    info.entries.push_back({item, value.empty() ? "unavailable" : value,
        source});
}

std::string read_linux_os_name(std::string &source)
{
#ifdef __ANDROID__
    char release[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.release", release) > 0) {
        source = "Android system property";
        return std::string("Android ") + release;
    }
#endif
    std::ifstream input("/etc/os-release");
    std::string line;
    while (std::getline(input, line)) {
        const std::string prefix = "PRETTY_NAME=";
        if (line.compare(0, prefix.size(), prefix) != 0) continue;
        std::string value = trim(line.substr(prefix.size()));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        source = "/etc/os-release";
        return value;
    }
    source = "uname";
    return "Linux";
}

std::string read_linux_cpu_model(std::string &source)
{
    std::ifstream input("/proc/cpuinfo");
    std::string line;
    const char *keys[] = {"model name", "hardware", "processor"};
    for (const char *key : keys) {
        input.clear();
        input.seekg(0);
        while (std::getline(input, line)) {
            const std::size_t separator = line.find(':');
            if (separator == std::string::npos) continue;
            std::string name = trim(line.substr(0, separator));
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (name != key) continue;
            const std::string value = trim(line.substr(separator + 1));
            if (!value.empty() && (name != "processor" ||
                    !std::all_of(value.begin(), value.end(),
                        [](unsigned char ch) { return std::isdigit(ch); }))) {
                source = "/proc/cpuinfo";
                return value;
            }
        }
    }

#ifdef __ANDROID__
    char product_model[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.product.model", product_model) > 0) {
        source = "Android system property";
        return product_model;
    }
#endif

    const char *fallback_paths[] = {
        "/sys/firmware/devicetree/base/model",
        "/proc/device-tree/model",
        "/sys/devices/soc0/machine",
        "/sys/devices/virtual/dmi/id/product_name",
    };
    for (const char *path : fallback_paths) {
        std::string value;
        if (!read_first_line(path, value)) continue;
        value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
        if (value.empty()) continue;
        std::string normalized = value;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (normalized == "to be filled by o.e.m." ||
            normalized == "default string" || normalized == "unknown")
            continue;
        source = path;
        return value;
    }

    input.clear();
    input.seekg(0);
    std::string implementer;
    std::string part;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) continue;
        const std::string name = trim(line.substr(0, separator));
        if (name == "CPU implementer")
            implementer = trim(line.substr(separator + 1));
        else if (name == "CPU part")
            part = trim(line.substr(separator + 1));
        if (!implementer.empty() && !part.empty()) break;
    }
    if (!implementer.empty() || !part.empty()) {
        if (implementer == "0x48") implementer = "HiSilicon (0x48)";
        source = "/proc/cpuinfo";
        if (implementer.empty()) return "ARM part " + part;
        if (part.empty()) return implementer;
        return implementer + ", ARM part " + part;
    }
    return "";
}

struct FrequencyInfo
{
    double base_hz = 0.0;
    double current_hz = 0.0;
    double maximum_hz = 0.0;
    std::string source;
};

#ifdef __linux__
bool find_linux_cache_level(int cpu, int requested_level, std::string &source)
{
    const std::string root = "/sys/devices/system/cpu/cpu" +
        std::to_string(cpu) + "/cache/";
    for (int index = 0; index < 32; ++index) {
        const std::string entry = root + "index" + std::to_string(index) + "/";
        std::string level;
        std::string type;
        if (!read_first_line(entry + "level", level) ||
            !read_first_line(entry + "type", type) ||
            level != std::to_string(requested_level) ||
            (type != "Data" && type != "Unified"))
            continue;
        source = "Linux sysfs cpu" + std::to_string(cpu) + "/cache/index" +
            std::to_string(index) + " (L" + level + " " + type + ")";
        return true;
    }
    return false;
}

std::uint16_t read_u16_le(const unsigned char *data)
{
    return static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8);
}

FrequencyInfo read_linux_frequency(int cpu)
{
    FrequencyInfo result;
    const std::string root = "/sys/devices/system/cpu/cpu" +
        std::to_string(cpu) + "/cpufreq/";
    double value = 0.0;
    if (read_positive_number(root + "base_frequency", value))
        result.base_hz = value * 1000.0;
    if (read_positive_number(root + "scaling_cur_freq", value) ||
        read_positive_number(root + "cpuinfo_cur_freq", value))
        result.current_hz = value * 1000.0;
    if (read_positive_number(root + "cpuinfo_max_freq", value) ||
        read_positive_number(root + "scaling_max_freq", value))
        result.maximum_hz = value * 1000.0;
    if (result.base_hz > 0.0 || result.current_hz > 0.0 ||
        result.maximum_hz > 0.0)
        result.source = "Linux cpufreq sysfs";

    if (result.base_hz == 0.0 || result.maximum_hz == 0.0) {
        std::ifstream input("/sys/firmware/dmi/entries/4-0/raw",
            std::ios::in | std::ios::binary);
        unsigned char record[24] = {0};
        if (input.read(reinterpret_cast<char *>(record), sizeof(record)) &&
            record[0] == 4 && record[1] >= sizeof(record)) {
            const double maximum_hz = read_u16_le(record + 0x14) * 1e6;
            const double configured_hz = read_u16_le(record + 0x16) * 1e6;
            if (result.base_hz == 0.0) result.base_hz = configured_hz;
            if (result.maximum_hz == 0.0) result.maximum_hz = maximum_hz;
            result.source = result.source.empty() ? "Linux SMBIOS sysfs" :
                result.source + "; Linux SMBIOS sysfs";
        }
    }
    return result;
}
#endif

#ifdef __APPLE__
bool read_sysctl_string(const char *name, std::string &value)
{
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0)
        return false;
    std::vector<char> buffer(size);
    if (sysctlbyname(name, buffer.data(), &size, nullptr, 0) != 0)
        return false;
    value.assign(buffer.data());
    return !value.empty();
}

FrequencyInfo read_macos_frequency()
{
    FrequencyInfo result;
    std::uint64_t frequency = 0;
    std::size_t size = sizeof(frequency);
    if (sysctlbyname("hw.cpufrequency", &frequency, &size, nullptr, 0) == 0) {
        result.base_hz = static_cast<double>(frequency);
        result.maximum_hz = static_cast<double>(frequency);
        result.source = "macOS hw.cpufrequency";
    }
    return result;
}
#endif

std::string format_frequency(const FrequencyInfo &frequency)
{
    std::vector<std::string> fields;
    if (frequency.base_hz > 0.0)
        fields.push_back("base " + format_ghz(frequency.base_hz));
    if (frequency.current_hz > 0.0)
        fields.push_back("current " + format_ghz(frequency.current_hz));
    if (frequency.maximum_hz > 0.0)
        fields.push_back("max/turbo " + format_ghz(frequency.maximum_hz));
    if (fields.empty()) return "unavailable";

    std::ostringstream output;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index != 0) output << ", ";
        output << fields[index];
    }
    return output.str();
}

} // namespace

SystemInfo collect_system_info(const std::vector<int> &selected_cores)
{
    SystemInfo info;
    struct utsname system_name = {};
    const bool have_uname = uname(&system_name) == 0;

    add_entry(info, "Sample Timestamp", current_timestamp(), "system clock");

#ifdef __linux__
    std::string os_source;
    add_entry(info, "OS", read_linux_os_name(os_source), os_source);
#elif defined(__APPLE__)
    std::string product_version;
    read_sysctl_string("kern.osproductversion", product_version);
    add_entry(info, "OS", product_version.empty() ? "macOS" :
        "macOS " + product_version, "sysctl kern.osproductversion");
#else
    add_entry(info, "OS", have_uname ? system_name.sysname : "unknown", "uname");
#endif
    add_entry(info, "Kernel", have_uname ?
        std::string(system_name.sysname) + " " + system_name.release : "unknown",
        "uname");
    add_entry(info, "Architecture", have_uname ? system_name.machine : "unknown",
        "uname");
    add_entry(info, "Compiler", compiler_name(), "compile-time macros");

#ifdef __linux__
    std::string cpu_model_source;
    add_entry(info, "CPU Model", read_linux_cpu_model(cpu_model_source),
        cpu_model_source.empty() ? "system interface unavailable" :
            cpu_model_source);
#elif defined(__APPLE__)
    std::string cpu_model;
    read_sysctl_string("machdep.cpu.brand_string", cpu_model);
    add_entry(info, "CPU Model", cpu_model, "sysctl machdep.cpu.brand_string");
#else
    add_entry(info, "CPU Model", "unavailable", "unsupported platform");
#endif

    add_entry(info, "Core Selection", format_cores(selected_cores),
        "--thread_pool");
    add_entry(info, "Core Migration", "unavailable", "not measured");
    std::string temperature_source;
    add_entry(info, "Temperature", read_temperature(temperature_source),
        temperature_source);
    const int cpu = selected_cores.empty() ? 0 : selected_cores.front();
#ifdef __linux__
    const FrequencyInfo frequency = read_linux_frequency(cpu);
#elif defined(__APPLE__)
    const FrequencyInfo frequency = read_macos_frequency();
#else
    const FrequencyInfo frequency;
#endif
    add_entry(info, "CPU Frequency", format_frequency(frequency),
        frequency.source.empty() ? "system interface unavailable" :
            frequency.source);

    for (int level = 1; level <= 3; ++level) {
        const CacheLevelInfo cache = detect_data_cache_level(cpu, level);
        std::string cache_source = cache.source;
        std::string cache_value = cache.bytes > 0 ?
            format_cache_capacity(cache.bytes) : "unavailable";
#ifdef __linux__
        if (cache.bytes == 0 &&
            find_linux_cache_level(cpu, level, cache_source))
            cache_value = "present; capacity not exposed";
#endif
        add_entry(info, "L" + std::to_string(level) + " Data/Unified Cache",
            cache_value, cache_source.empty() ?
                "system interface unavailable" : cache_source);
    }
    return info;
}

void populate_system_info_table(const SystemInfo &info, Table &table)
{
    std::vector<std::string> row(3);
    row[0] = "Item";
    row[1] = "Value";
    row[2] = "Source";
    table.setColumnNum(row.size());
    table.addOneItem(row);
    for (const SystemInfoEntry &entry : info.entries) {
        row[0] = entry.item;
        row[1] = entry.value;
        row[2] = entry.source;
        table.addOneItem(row);
    }
}

} // namespace cpufb