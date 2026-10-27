#ifndef CPUFB_BENCHMARK_TABLES_HPP
#define CPUFB_BENCHMARK_TABLES_HPP

#include <string>
#include <vector>

#include "cache/cache_bandwidth.hpp"
#include "cache/cache_report.hpp"

class Table;
struct tpool;
typedef struct tpool tpool_t;

namespace cpufb {

// Column set of the load table.  The architectures differ here on purpose:
// ARM64 and riscv64 name the capacity's source and the cycle source (their
// capacities may come from the probe and their cycles from a clock
// estimate); x86-64 shows the reported and the measured size side by side.
enum LoadTableLayout
{
    LOAD_TABLE_WITH_SOURCES, // 10 columns
    LOAD_TABLE_REPORTED_AND_MEASURED // 8 columns
};

// The five result tables in the order print_and_save_benchmark_tables()
// expects: compute, load (L1/L2 bandwidth), cache, frequency, multi_issue.
// sve_columns adds the two SVE IPC columns to the frequency table.
void init_benchmark_tables(
    std::vector<Table *> &tables, bool sve_columns, LoadTableLayout load_layout);
void init_frequency_table(Table &table, bool sve_columns);

// One row of the load table: the kernel's bandwidth on the pool workers at a
// working set inside the level under test (cache_bandwidth.hpp).  Level
// capacities are the reported ones when exposed, the measured ones otherwise;
// the clock is the mean of the frequency table's cores when the PMU cannot
// count.  `label` is the first column ("L1 Cache", "--------").
void append_load_bandwidth_row(Table &table, LoadTableLayout layout,
    const std::string &label, const LoadKernel &kernel, int cache_level,
    const CacheData &capacities, tpool_t *pool,
    const std::vector<double> &clock_ghz);

// `value` with `significant_digits` digits and a unit, or "-" when not
// positive; and with a fixed number of decimals.
std::string format_or_dash(
    double value, int significant_digits, const std::string &unit);
std::string format_fixed(double value, int decimals, const std::string &unit);

// Once per run: per-kernel cycles came from the clock named, not a counter.
void warn_estimated_cycles_once(const std::string &clock_source);

} // namespace cpufb

#endif
