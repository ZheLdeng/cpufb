#ifndef CPUFB_CLI_HPP
#define CPUFB_CLI_HPP

#include "table.hpp"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cpufb::cli {

struct BenchmarkInfo
{
    std::string isa;
    std::string instruction;
    std::string metric;
    bool is_latency;
    int pair_index;

    BenchmarkInfo(const std::string &isa_value,
        const std::string &instruction_value, const std::string &metric_value);
};

typedef std::vector<BenchmarkInfo> BenchmarkCatalog;

void pair_benchmark_latencies(BenchmarkCatalog &catalog);

struct BenchmarkFilter
{
    std::set<std::string> include_isa;
    std::set<std::string> exclude_isa;
    std::set<std::string> include_test;
    std::set<std::string> exclude_test;
};

enum SaveFormat { SAVE_FORMAT_TXT, SAVE_FORMAT_CSV };

enum BenchMode { BENCH_MODE_ALL, BENCH_MODE_CACHE, BENCH_MODE_COMPUTE };

struct SaveOptions
{
    bool enabled;
    bool format_set;
    std::string path;
    SaveFormat format;

    SaveOptions();
};

struct CliOptions
{
    std::vector<int> thread_pool;
    std::uint32_t idle_time;
    BenchmarkFilter filter;
    bool list_categories;
    bool list_instructions;
    std::string sweep_instruction;
    bool memory_bandwidth;
    std::uint64_t memory_size_mib;
    std::uint32_t memory_repetitions;
    bool memory_size_set;
    bool memory_repetitions_set;
    SaveOptions save;
    bool thread_pool_set;
    BenchMode mode;
    bool mode_explicit;
    bool include_test_explicit;
    std::uint32_t loop_scale;
    std::uint32_t bench_limit;

    CliOptions();
};

bool parse_cli_options(int argc, char *argv[], CliOptions &options);
bool finalize_save_options(SaveOptions &options);
bool validate_memory_bandwidth_options(
    const CliOptions &options, bool architecture_supported);

void initialize_system_information(const std::vector<int> &selected_cores);
void print_system_information();

std::string trim_arg_value(const std::string &value);
std::string normalize_filter_value(std::string value);
// What a registered row measures.  The metric string is only a display unit;
// it is mapped to a kind once, at registration, and everything else switches
// on the kind.
enum BenchmarkKind { BENCHMARK_COMPUTE, BENCHMARK_LOAD, BENCHMARK_MULTI_ISSUE };
BenchmarkKind benchmark_kind_from_metric(const std::string &metric);
std::string get_benchmark_test_type(const std::string &metric);
bool should_run_test(
    const BenchmarkFilter &filter, const std::string &test_type);
bool should_run_benchmark(const BenchmarkFilter &filter, const std::string &isa,
    const std::string &metric);
bool benchmark_needs_freq(const BenchmarkFilter &filter);
bool should_run_standalone_warmup(const BenchmarkFilter &filter);
bool is_latency_benchmark(const std::string &instruction);
bool is_compute_instruction_candidate(const BenchmarkInfo &item);

void print_benchmark_categories(const BenchmarkCatalog &catalog);
void print_benchmark_instructions(const BenchmarkCatalog &catalog);
bool validate_benchmark_filter(
    const BenchmarkFilter &filter, const BenchmarkCatalog &catalog);

bool save_table_sections(const SaveOptions &options,
    const std::vector<std::pair<std::string, const Table *>> &sections);
bool print_and_save_benchmark_tables(const BenchmarkFilter &filter,
    const SaveOptions &options, const std::vector<Table *> &tables);

std::string format_thread_pool_prefix(
    const std::vector<int> &threads, std::size_t count);
std::string format_perf_value(double perf, const std::string &metric);
std::string format_ratio_value(double value);
std::string format_percent_value(double value);

struct SweepSample
{
    double performance;
    double ipc;
    std::string latency;

    SweepSample();
};

struct SweepConfig
{
    std::string ipc_column;
    std::string latency_column;
    bool print_banner;
    bool include_metadata;

    SweepConfig();
};

typedef bool (*SweepPrepareCallback)(
    const std::vector<int> &threads, int benchmark_index, void *context);
typedef bool (*SweepMeasureCallback)(const std::vector<int> &active_threads,
    std::uint32_t idle_time, int benchmark_index, int latency_index,
    SweepSample &sample, void *context);

bool run_instruction_sweep(const std::vector<int> &threads,
    std::uint32_t idle_time, const std::string &instruction,
    const BenchmarkCatalog &catalog, const SaveOptions &save_options,
    const SweepConfig &config, SweepPrepareCallback prepare,
    SweepMeasureCallback measure, void *context);

} // namespace cpufb::cli

#endif
