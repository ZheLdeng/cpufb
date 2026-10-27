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

// The five result tables in the order print_and_save_benchmark_tables()
// expects: compute, load (L1/L2 bandwidth), cache, frequency, multi_issue.
// One layout for every architecture; sve_columns adds the two SVE IPC
// columns to the frequency table.
void init_benchmark_tables(std::vector<Table *> &tables, bool sve_columns);
void init_frequency_table(Table &table, bool sve_columns);

// One row of the load table: the kernel's bandwidth on the pool workers at a
// working set inside the level under test (cache_bandwidth.hpp).  Level
// capacities are the reported ones when exposed, the measured ones otherwise;
// the clock is the mean of the frequency table's cores when the PMU cannot
// count.  `label` is the first column ("L1 Cache", "--------").
void append_load_bandwidth_row(Table &table, const std::string &label,
    const LoadKernel &kernel, int cache_level, const CacheData &capacities,
    tpool_t *pool, const std::vector<double> &clock_ghz);

// `value` with `significant_digits` digits and a unit, or "-" when not
// positive; and with a fixed number of decimals.
std::string format_or_dash(
    double value, int significant_digits, const std::string &unit);
std::string format_fixed(double value, int decimals, const std::string &unit);

// Once per run: per-kernel cycles came from the clock named, not a counter.
void warn_estimated_cycles_once(const std::string &clock_source);

} // namespace cpufb

#endif
