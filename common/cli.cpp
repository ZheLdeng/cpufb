#include "cli.hpp"

#include "thread_pool.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

namespace cpufb_cli {

namespace {

const char *const kTestCategories[] = {
    "compute", "load", "cache", "freq", "multi_issue"
};
const size_t kTestCategoryCount =
    sizeof(kTestCategories) / sizeof(kTestCategories[0]);

void parse_filter_list(const char *value, set<string> &target)
{
    stringstream ss(value);
    string item;
    while (getline(ss, item, ',')) {
        item = normalize_filter_value(item);
        if (!item.empty()) target.insert(item);
    }
}

bool filter_allows_value(const set<string> &include,
    const set<string> &exclude,
    const string &value)
{
    if (exclude.find(value) != exclude.end()) return false;
    return include.empty() || include.find(value) != include.end();
}

bool parse_save_format(const char *value, SaveFormat &format)
{
    string normalized = normalize_filter_value(value);
    if (normalized == "txt" || normalized == "tsv") {
        format = SAVE_FORMAT_TXT;
        return true;
    }
    if (normalized == "csv") {
        format = SAVE_FORMAT_CSV;
        return true;
    }
    return false;
}

bool ends_with_case_insensitive(const string &value, const string &suffix)
{
    if (suffix.size() > value.size()) return false;
    return normalize_filter_value(value.substr(value.size() - suffix.size())) ==
        normalize_filter_value(suffix);
}

SaveFormat infer_save_format_from_path(const string &path)
{
    return ends_with_case_insensitive(path, ".csv") ?
        SAVE_FORMAT_CSV : SAVE_FORMAT_TXT;
}

const char *save_format_name(SaveFormat format)
{
    return format == SAVE_FORMAT_CSV ? "csv" : "txt";
}

vector<int> find_compute_instruction_matches(const string &instruction,
    const BenchmarkCatalog &catalog)
{
    vector<int> exact_matches;
    vector<int> partial_matches;
    string needle = normalize_filter_value(instruction);
    if (needle.empty()) return exact_matches;

    for (int i = 0; i < static_cast<int>(catalog.size()); ++i) {
        const BenchmarkInfo &item = catalog[i];
        if (!is_compute_instruction_candidate(item)) continue;
        string candidate = normalize_filter_value(item.instruction);
        if (candidate == needle)
            exact_matches.push_back(i);
        else if (candidate.find(needle) != string::npos)
            partial_matches.push_back(i);
    }
    return exact_matches.empty() ? partial_matches : exact_matches;
}

int find_latency_pair_index(const BenchmarkCatalog &catalog,
    int benchmark_index,
    bool allow_isa_fallback)
{
    const BenchmarkInfo &selected = catalog[benchmark_index];
    string latency_instruction = selected.instruction + "_latency";
    string selected_isa = normalize_filter_value(selected.isa);

    for (int i = 0; i < static_cast<int>(catalog.size()); ++i) {
        const BenchmarkInfo &candidate = catalog[i];
        if (candidate.instruction == latency_instruction &&
            normalize_filter_value(candidate.isa) == selected_isa)
            return i;
    }
    if (allow_isa_fallback) {
        for (int i = 0; i < static_cast<int>(catalog.size()); ++i) {
            if (catalog[i].instruction == latency_instruction) return i;
        }
    }
    return -1;
}

} // namespace

BenchmarkInfo::BenchmarkInfo(const string &isa_value,
    const string &instruction_value,
    const string &metric_value) :
    isa(isa_value),
    instruction(instruction_value),
    metric(metric_value)
{
}

SaveOptions::SaveOptions() :
    enabled(false),
    format_set(false),
    format(SAVE_FORMAT_TXT)
{
}

CliOptions::CliOptions() :
    idle_time(0),
    list_categories(false),
    list_instructions(false),
    thread_pool_set(false)
{
}

bool parse_cli_options(int argc, char *argv[], CliOptions &options)
{
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--thread_pool=", 14) == 0) {
            parse_thread_pool(argv[i] + 14, options.thread_pool);
            options.thread_pool_set = true;
        } else if (strncmp(argv[i], "--idle_time=", 12) == 0) {
            options.idle_time = static_cast<uint32_t>(atoi(argv[i] + 12));
        } else if (strncmp(argv[i], "--include-isa=", 14) == 0) {
            parse_filter_list(argv[i] + 14, options.filter.include_isa);
        } else if (strncmp(argv[i], "--exclude-isa=", 14) == 0) {
            parse_filter_list(argv[i] + 14, options.filter.exclude_isa);
        } else if (strncmp(argv[i], "--include-test=", 15) == 0) {
            parse_filter_list(argv[i] + 15, options.filter.include_test);
        } else if (strncmp(argv[i], "--exclude-test=", 15) == 0) {
            parse_filter_list(argv[i] + 15, options.filter.exclude_test);
        } else if (strcmp(argv[i], "--list-categories") == 0) {
            options.list_categories = true;
        } else if (strcmp(argv[i], "--list-instructions") == 0) {
            options.list_instructions = true;
        } else if (strncmp(argv[i], "--sweep-instruction=", 20) == 0) {
            options.sweep_instruction = argv[i] + 20;
        } else if (strncmp(argv[i], "--scale-instruction=", 20) == 0) {
            options.sweep_instruction = argv[i] + 20;
        } else if (strncmp(argv[i], "--save=", 7) == 0) {
            options.save.enabled = true;
            options.save.path = argv[i] + 7;
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            options.save.enabled = true;
            options.save.path = argv[i] + 9;
        } else if (strncmp(argv[i], "--save-format=", 14) == 0) {
            if (!parse_save_format(argv[i] + 14, options.save.format)) {
                cerr << "Error: unsupported --save-format value '"
                     << argv[i] + 14 << "'. Use txt or csv." << endl;
                return false;
            }
            options.save.format_set = true;
        } else if (strncmp(argv[i], "--output-format=", 16) == 0) {
            if (!parse_save_format(argv[i] + 16, options.save.format)) {
                cerr << "Error: unsupported --output-format value '"
                     << argv[i] + 16 << "'. Use txt or csv." << endl;
                return false;
            }
            options.save.format_set = true;
        }
    }
    return true;
}

bool finalize_save_options(SaveOptions &options)
{
    if (!options.enabled) return true;
    options.path = trim_arg_value(options.path);
    if (options.path.empty()) {
        cerr << "Error: --save path must not be empty." << endl;
        return false;
    }
    if (!options.format_set)
        options.format = infer_save_format_from_path(options.path);
    return true;
}

string trim_arg_value(const string &value)
{
    size_t start = value.find_first_not_of(" \t\n\r");
    if (start == string::npos) return "";
    size_t end = value.find_last_not_of(" \t\n\r");
    return value.substr(start, end - start + 1);
}

string normalize_filter_value(string value)
{
    value = trim_arg_value(value);
    transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return value;
}

string get_benchmark_test_type(const string &metric)
{
    if (metric.find("Byte/") != string::npos) return "load";
    if (metric.find("IPC") != string::npos) return "multi_issue";
    return "compute";
}

bool should_run_test(const BenchmarkFilter &filter, const string &test_type)
{
    return filter_allows_value(filter.include_test,
        filter.exclude_test,
        test_type);
}

bool should_run_benchmark(const BenchmarkFilter &filter,
    const string &isa,
    const string &metric)
{
    string test_type = get_benchmark_test_type(metric);
    if (!should_run_test(filter, test_type)) return false;
    if (test_type == "compute") {
        return filter_allows_value(filter.include_isa,
            filter.exclude_isa,
            normalize_filter_value(isa));
    }
    return true;
}

bool benchmark_needs_freq(const BenchmarkFilter &filter)
{
    return should_run_test(filter, "compute") ||
        should_run_test(filter, "load") ||
        should_run_test(filter, "multi_issue") ||
        should_run_test(filter, "freq");
}

bool should_run_standalone_warmup(const BenchmarkFilter &filter)
{
    return filter.include_test.size() == 1 || !filter.include_isa.empty();
}

bool is_latency_benchmark(const string &instruction)
{
    return instruction.find("_latency") != string::npos;
}

bool is_compute_instruction_candidate(const BenchmarkInfo &item)
{
    return get_benchmark_test_type(item.metric) == "compute" &&
        !is_latency_benchmark(item.instruction);
}

void print_benchmark_categories(const BenchmarkCatalog &catalog)
{
    set<string> isa_categories;
    for (const BenchmarkInfo &item : catalog) {
        if (get_benchmark_test_type(item.metric) == "compute")
            isa_categories.insert(item.isa);
    }

    cout << "Test categories:" << endl;
    for (size_t i = 0; i < kTestCategoryCount; ++i)
        cout << "  " << kTestCategories[i] << endl;
    cout << "ISA categories:" << endl;
    for (const string &isa : isa_categories) cout << "  " << isa << endl;
}

void print_benchmark_instructions(const BenchmarkCatalog &catalog)
{
    Table table;
    vector<string> row(3);
    row[0] = "Instruction Set";
    row[1] = "Core Computation";
    row[2] = "Metric";
    table.setColumnNum(row.size());
    table.addOneItem(row);
    for (const BenchmarkInfo &item : catalog) {
        if (!is_compute_instruction_candidate(item)) continue;
        row[0] = item.isa;
        row[1] = item.instruction;
        row[2] = item.metric;
        table.addOneItem(row);
    }
    table.print();
}

bool validate_benchmark_filter(const BenchmarkFilter &filter,
    const BenchmarkCatalog &catalog)
{
    set<string> valid_tests(kTestCategories,
        kTestCategories + kTestCategoryCount);
    set<string> valid_isas;
    for (const BenchmarkInfo &item : catalog) {
        if (get_benchmark_test_type(item.metric) == "compute")
            valid_isas.insert(normalize_filter_value(item.isa));
    }

    const set<string> *test_sets[] = {
        &filter.include_test, &filter.exclude_test
    };
    for (size_t i = 0; i < 2; ++i) {
        for (const string &value : *test_sets[i]) {
            if (valid_tests.find(value) == valid_tests.end()) {
                cerr << "Error: unknown test category '" << value << "'." << endl;
                return false;
            }
        }
    }

    const set<string> *isa_sets[] = {
        &filter.include_isa, &filter.exclude_isa
    };
    for (size_t i = 0; i < 2; ++i) {
        for (const string &value : *isa_sets[i]) {
            if (valid_isas.find(value) == valid_isas.end()) {
                cerr << "Error: unavailable ISA category '" << value << "'." << endl;
                return false;
            }
        }
    }
    return true;
}

bool save_table_sections(const SaveOptions &options,
    const vector<pair<string, const Table*> > &sections)
{
    if (!options.enabled) return true;
    ofstream out(options.path.c_str());
    if (!out) {
        cerr << "Error: failed to open save output '" << options.path << "'."
             << endl;
        return false;
    }

    if (options.format == SAVE_FORMAT_CSV) {
        for (size_t i = 0; i < sections.size(); ++i)
            sections[i].second->writeCompact(out, ',', sections[i].first);
    } else {
        for (size_t i = 0; i < sections.size(); ++i) {
            if (i != 0) out << '\n';
            out << "[" << sections[i].first << "]\n";
            sections[i].second->writeCompact(out, '\t');
        }
    }
    cout << "Saved " << save_format_name(options.format)
         << " output: " << options.path << endl;
    return true;
}

bool print_and_save_benchmark_tables(const BenchmarkFilter &filter,
    const SaveOptions &options,
    const vector<Table*> &tables)
{
    if (tables.size() < kTestCategoryCount) {
        cerr << "Error: incomplete benchmark output table set." << endl;
        return false;
    }

    vector<pair<string, const Table*> > sections;
    for (size_t i = 0; i < kTestCategoryCount; ++i) {
        if (!should_run_test(filter, kTestCategories[i])) continue;
        tables[i]->print();
        sections.push_back(make_pair(string(kTestCategories[i]), tables[i]));
    }
    return save_table_sections(options, sections);
}

string format_thread_pool_prefix(const vector<int> &threads, size_t count)
{
    stringstream ss;
    ss << "[";
    for (size_t i = 0; i < count; ++i) {
        if (i != 0) ss << ",";
        ss << threads[i];
    }
    ss << "]";
    return ss.str();
}

string format_perf_value(double perf, const string &metric)
{
    char unit = 'G';
    if (perf > 1e12) {
        unit = 'T';
        perf /= 1e12;
    } else {
        perf /= 1e9;
    }
    stringstream ss;
    ss << setprecision(5) << perf << " " << unit << metric;
    return ss.str();
}

string format_ratio_value(double value)
{
    stringstream ss;
    ss << setprecision(4) << value << "x";
    return ss.str();
}

string format_percent_value(double value)
{
    stringstream ss;
    ss << setprecision(4) << value * 100.0 << "%";
    return ss.str();
}

SweepSample::SweepSample() :
    performance(0.0),
    ipc(0.0),
    latency("-")
{
}

SweepConfig::SweepConfig() :
    ipc_column("IPC"),
    latency_column("Latency"),
    print_banner(false),
    include_metadata(false),
    allow_latency_isa_fallback(false)
{
}

bool run_instruction_sweep(const vector<int> &threads,
    std::uint32_t idle_time,
    const string &instruction,
    const BenchmarkCatalog &catalog,
    const SaveOptions &save_options,
    const SweepConfig &config,
    SweepPrepareCallback prepare,
    SweepMeasureCallback measure,
    void *context)
{
    if (catalog.empty()) {
        cout << "Sorry, there's no any supported SIMD isa." << endl;
        return false;
    }

    vector<int> matches = find_compute_instruction_matches(instruction, catalog);
    if (matches.empty()) {
        cerr << "Error: no compute instruction matched '" << instruction << "'."
             << endl;
        cerr << "Use --list-instructions to list valid Core Computation names."
             << endl;
        return false;
    }
    if (matches.size() > 1) {
        cerr << "Error: instruction name '" << instruction
             << "' matched multiple compute instructions:" << endl;
        for (int index : matches) {
            cerr << "  " << catalog[index].isa << " / "
                 << catalog[index].instruction << endl;
        }
        cerr << "Use the full Core Computation name to select one instruction."
             << endl;
        return false;
    }

    int benchmark_index = matches[0];
    int latency_index = find_latency_pair_index(catalog,
        benchmark_index,
        config.allow_latency_isa_fallback);
    const BenchmarkInfo &selected = catalog[benchmark_index];

    if (config.print_banner) {
        cout << "Instruction Sweep: " << selected.isa << " / "
             << selected.instruction << endl;
        cout << "Thread Pool Binding:";
        for (int cpu : threads) cout << " " << cpu;
        cout << endl;
    }

    if (prepare != NULL && !prepare(threads, benchmark_index, context))
        return false;

    Table table;
    vector<string> row(8);
    row[0] = "Cores";
    row[1] = "Thread Pool";
    row[2] = "Peak Performance";
    row[3] = "Peak/Core";
    row[4] = "Speedup";
    row[5] = "Efficiency";
    row[6] = config.ipc_column;
    row[7] = config.latency_column;
    table.setColumnNum(row.size());
    table.addOneItem(row);

    double baseline = 0.0;
    for (size_t cores = 1; cores <= threads.size(); ++cores) {
        vector<int> active_threads(threads.begin(), threads.begin() + cores);

        SweepSample sample;
        if (measure == NULL || !measure(active_threads,
                idle_time,
                benchmark_index,
                latency_index,
                sample,
                context)) {
            cerr << "Error: instruction sweep measurement failed." << endl;
            return false;
        }

        if (cores == 1) baseline = sample.performance;
        double speedup = baseline > 0.0 ? sample.performance / baseline : 0.0;
        double efficiency = speedup / cores;
        row[0] = to_string(cores);
        row[1] = format_thread_pool_prefix(threads, cores);
        row[2] = format_perf_value(sample.performance, selected.metric);
        row[3] = format_perf_value(sample.performance / cores, selected.metric);
        row[4] = format_ratio_value(speedup);
        row[5] = format_percent_value(efficiency);
        row[6] = to_string(sample.ipc);
        row[7] = sample.latency.empty() ? "-" : sample.latency;
        table.addOneItem(row);
    }
    table.print();

    Table metadata;
    vector<pair<string, const Table*> > sections;
    if (config.include_metadata && save_options.enabled) {
        vector<string> metadata_row(2);
        metadata_row[0] = "Item";
        metadata_row[1] = "Value";
        metadata.setColumnNum(metadata_row.size());
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Instruction Set";
        metadata_row[1] = selected.isa;
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Core Computation";
        metadata_row[1] = selected.instruction;
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Metric";
        metadata_row[1] = selected.metric;
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Thread Pool";
        metadata_row[1] = format_thread_pool_prefix(threads, threads.size());
        metadata.addOneItem(metadata_row);
        sections.push_back(make_pair(string("sweep_metadata"), &metadata));
    }
    sections.push_back(make_pair(string("instruction_sweep"), &table));
    return save_table_sections(save_options, sections);
}

} // namespace cpufb_cli
