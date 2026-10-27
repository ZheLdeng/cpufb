#include "benchmark_tables.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "issue/issue_probe.hpp"
#include "sys/thread_pool.hpp"
#include "table.hpp"

namespace cpufb {

namespace {

void set_header(Table &table, const std::vector<std::string> &columns)
{
    std::vector<std::string> head = columns;
    table.setColumnNum(static_cast<int>(head.size()));
    table.addOneItem(head);
}

} // namespace

void init_frequency_table(Table &table, bool sve_columns)
{
    std::vector<std::string> head = {"Core ID", "Reported Max Freq",
        "Test Freq", "IPC(FSU32)", "IPC(FSU64)", "IPC(LSU ldr)",
        "Counter Source"};
    if (sve_columns) {
        head.push_back("IPC(SVE32)");
        head.push_back("IPC(SVE64)");
    }
    set_header(table, head);
}

void init_benchmark_tables(std::vector<Table *> &tables, bool sve_columns)
{
    tables.resize(5);
    for (Table *&table : tables) table = new Table();
    set_header(*tables[0], {"Instruction Set", "Core Computation",
                               "Peak Performance", "IPC", "Latency (cycles)"});
    set_header(*tables[1],
        {"Cache Level", "Core Instruction", "Bandwidth (per core)",
            "Cache Capacity", "Workset", "Capacity Source", "Bandwidth (GB/s)",
            "Cycle Source", "Access IPC", "Bytes/Access"});
    set_header(*tables[2], {"Item", "Topology / Core", "Probe / Kernel",
                               "Median Bandwidth", "Workset", "Measurement"});
    init_frequency_table(*tables[3], sve_columns);
    issue_probe_table_header(*tables[4]);
}

std::string format_or_dash(
    double value, int significant_digits, const std::string &unit)
{
    if (!(value > 0.0)) return "-";
    std::ostringstream text;
    text << std::setprecision(significant_digits) << value << unit;
    return text.str();
}

std::string format_fixed(double value, int decimals, const std::string &unit)
{
    std::ostringstream text;
    text << std::fixed << std::setprecision(decimals) << value << unit;
    return text.str();
}

void warn_estimated_cycles_once(const std::string &clock_source)
{
    static bool warned = false;
    if (warned) return;
    warned = true;
    std::cerr << "Warning: per-kernel hardware cycle counts are unavailable; "
              << "IPC and latency are estimated from elapsed time x "
              << clock_source << " frequency.";
#ifdef __linux__
    std::cerr << " Lower /proc/sys/kernel/perf_event_paranoid or run with "
                 "CAP_PERFMON for counted cycles.";
#endif
    std::cerr << std::endl;
}

void append_load_bandwidth_row(Table &table, const std::string &label,
    const LoadKernel &kernel, int cache_level, const CacheData &capacities,
    tpool_t *pool, const std::vector<double> &clock_ghz)
{
    auto capacity_kib = [](int reported, int measured) {
        return static_cast<size_t>(reported > 0 ? reported : measured);
    };
    const size_t l1_kib =
        capacity_kib(capacities.theory_L1, capacities.test_L1);
    const size_t l2_kib =
        capacity_kib(capacities.theory_L2, capacities.test_L2);
    const bool is_l1 = cache_level == 1;
    const size_t workset = is_l1
        ? cache_level_workset(0, l1_kib * 1024)
        : cache_level_workset(l1_kib * 1024, l2_kib * 1024);
    const int reported = is_l1 ? capacities.theory_L1 : capacities.theory_L2;
    const std::string &reported_source =
        is_l1 ? capacities.theory_L1_source : capacities.theory_L2_source;
    const std::string capacity_source = reported > 0
        ? (reported_source.empty() ? "OS topology" : reported_source)
        : "cache probe";

    // Fallback clock: the mean over the measured cores (macOS fills one).
    double clock_hz = 0.0;
    int clocks = 0;
    for (const double ghz : clock_ghz)
        if (ghz > 0.0) {
            clock_hz += ghz * 1e9;
            ++clocks;
        }
    if (clocks > 0) clock_hz /= clocks;

    const CacheBandwidth bandwidth =
        measure_cache_bandwidth(kernel, workset, pool, clock_hz);
    if (bandwidth.cycle_source == "time x clock")
        warn_estimated_cycles_once("the frequency table's");

    std::string gb_per_second = format_or_dash(bandwidth.gb_per_second, 5, " GB/s");
    if (bandwidth.worker_count > 1)
        gb_per_second += " (" + std::to_string(bandwidth.worker_count) + " cores)";

    std::vector<std::string> row = {label, kernel.name,
        format_or_dash(bandwidth.bytes_per_cycle, 5, " Byte/Cycle"),
        std::to_string(is_l1 ? l1_kib : l2_kib) + " KiB",
        std::to_string(bandwidth.workset_bytes / 1024) + " KiB",
        capacity_source, gb_per_second,
        bandwidth.cycle_source.empty() ? "-" : bandwidth.cycle_source,
        format_or_dash(bandwidth.load_ipc, 3, ""),
        std::to_string(kernel.bytes_per_load) + " B"};
    table.addOneItem(row);
}

} // namespace cpufb
