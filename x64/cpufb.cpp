#include "table.hpp"
#include "thread_pool.hpp"
// #include "./kernel/compute.hpp"
#include<compute.hpp>
#include<load.hpp>
#include<frequency.hpp>
#include<common.hpp>
#include<multiple_issue.hpp>

#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <vector>
#include <set>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

#if defined(_AMX_INT8_) || defined(_AMX_BF16_)
#include <sys/syscall.h>
#define _AMX_TILE_
#endif

using namespace std;
extern vector<double> freq;
static struct CacheData cache_size;


#define LOOP_INCREASE 256
#define LOOP_DECREASE 32

#ifdef _AMX_TILE_
struct
{
    uint8_t palette_id;
    uint8_t start_row;
    uint8_t reserved_0[14];
    uint16_t colsb[16];
    uint8_t rows[16];
} __tilecfg;

void init_tile_cfg()
{
    int i;
    __tilecfg.palette_id = 1;
    __tilecfg.start_row = 0;
    for (i = 0; i < 14; i++)
    {
        __tilecfg.reserved_0[i] = 0;
    }
    for (i = 0; i < 8; i++)
    {
        __tilecfg.colsb[i] = 64;
        __tilecfg.rows[i] = 16;
    }
    for (; i < 16; i++)
    {
        __tilecfg.colsb[i] = 0;
        __tilecfg.rows[i] = 0;
    }
}
#endif

typedef struct
{
    std::string isa;
    std::string type;
    std::string dim;
    int64_t loop_time;
    int64_t comp_pl; // Mathematical/element operations per outer asm loop.
    int64_t inst_pl; // Benchmarked instructions per outer asm loop.
    void *params;
    void (*bench)(int64_t, void*);
} cpubm_t;
typedef struct
{
    float* cache_data;
    int inner_loop;
    int loop_time;
    void (*bench)(float*, int, int64_t);
} cache_bm_t;
static vector<cpubm_t> bm_list;
static double g_latency = 0.0;

typedef struct
{
    double perf;
    double ipc;
} ComputeResult;

typedef struct
{
    set<string> include_isa;
    set<string> exclude_isa;
    set<string> include_test;
    set<string> exclude_test;
} BenchmarkFilter;

typedef enum
{
    SAVE_FORMAT_TXT,
    SAVE_FORMAT_CSV
} SaveFormat;

struct SaveOptions
{
    bool enabled;
    bool format_set;
    string path;
    SaveFormat format;

    SaveOptions() : enabled(false), format_set(false), format(SAVE_FORMAT_TXT) {}
};

static string trim_arg_value(const string &value)
{
    size_t start = value.find_first_not_of(" \t\n\r");
    if (start == string::npos) return "";
    size_t end = value.find_last_not_of(" \t\n\r");
    return value.substr(start, end - start + 1);
}

static string normalize_filter_value(string value)
{
    value = trim_arg_value(value);
    transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return value;
}

static void parse_filter_list(const char *value, set<string> &target)
{
    stringstream ss(value);
    string item;
    while (getline(ss, item, ',')) {
        item = normalize_filter_value(item);
        if (!item.empty()) target.insert(item);
    }
}

static bool filter_allows_value(const set<string> &include,
    const set<string> &exclude, const string &value)
{
    if (exclude.find(value) != exclude.end()) return false;
    return include.empty() || include.find(value) != include.end();
}

static string get_benchmark_test_type(const cpubm_t &item)
{
    if (item.dim.find("Byte/") != string::npos) return "load";
    if (item.dim.find("IPC") != string::npos) return "multi_issue";
    return "compute";
}

static bool should_run_test(const BenchmarkFilter &filter, const string &test_type)
{
    return filter_allows_value(filter.include_test, filter.exclude_test, test_type);
}

static bool should_run_benchmark(const BenchmarkFilter &filter, const cpubm_t &item)
{
    string test_type = get_benchmark_test_type(item);
    if (!should_run_test(filter, test_type)) return false;
    if (test_type == "compute") {
        return filter_allows_value(filter.include_isa, filter.exclude_isa,
            normalize_filter_value(item.isa));
    }
    return true;
}

static bool benchmark_needs_freq(const BenchmarkFilter &filter)
{
    return should_run_test(filter, "compute") || should_run_test(filter, "load") ||
        should_run_test(filter, "multi_issue") || should_run_test(filter, "freq");
}

static bool ends_with_case_insensitive(const string &value, const string &suffix)
{
    if (suffix.size() > value.size()) return false;
    return normalize_filter_value(value.substr(value.size() - suffix.size())) ==
        normalize_filter_value(suffix);
}

static bool parse_save_format(const char *value, SaveFormat &format)
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

static SaveFormat infer_save_format_from_path(const string &path)
{
    return ends_with_case_insensitive(path, ".csv") ? SAVE_FORMAT_CSV : SAVE_FORMAT_TXT;
}

static bool is_latency_benchmark(const cpubm_t &item)
{
    return item.type.find("_latency") != string::npos;
}

static bool is_compute_instruction_candidate(const cpubm_t &item)
{
    return get_benchmark_test_type(item) == "compute" && !is_latency_benchmark(item);
}

static void print_benchmark_categories()
{
    set<string> isa_categories;
    for (const cpubm_t &item : bm_list) {
        if (get_benchmark_test_type(item) == "compute") isa_categories.insert(item.isa);
    }
    cout << "Test categories:\n  compute\n  load\n  cache\n  freq\n  multi_issue" << endl;
    cout << "ISA categories:" << endl;
    for (const string &isa : isa_categories) cout << "  " << isa << endl;
}

static void print_benchmark_instructions()
{
    Table table;
    vector<string> row(3);
    row[0] = "Instruction Set";
    row[1] = "Core Computation";
    row[2] = "Metric";
    table.setColumnNum(row.size());
    table.addOneItem(row);
    for (const cpubm_t &item : bm_list) {
        if (!is_compute_instruction_candidate(item)) continue;
        row[0] = item.isa;
        row[1] = item.type;
        row[2] = item.dim;
        table.addOneItem(row);
    }
    table.print();
}

static bool validate_benchmark_filter(const BenchmarkFilter &filter)
{
    const char *test_names[] = {"compute", "load", "cache", "freq", "multi_issue"};
    set<string> valid_tests(test_names, test_names + 5);
    set<string> valid_isas;
    for (const cpubm_t &item : bm_list) {
        if (get_benchmark_test_type(item) == "compute")
            valid_isas.insert(normalize_filter_value(item.isa));
    }
    const set<string> *test_sets[] = {&filter.include_test, &filter.exclude_test};
    for (int i = 0; i < 2; ++i) {
        for (const string &value : *test_sets[i]) {
            if (valid_tests.find(value) == valid_tests.end()) {
                cerr << "Error: unknown test category '" << value << "'." << endl;
                return false;
            }
        }
    }
    const set<string> *isa_sets[] = {&filter.include_isa, &filter.exclude_isa};
    for (int i = 0; i < 2; ++i) {
        for (const string &value : *isa_sets[i]) {
            if (valid_isas.find(value) == valid_isas.end()) {
                cerr << "Error: unavailable ISA category '" << value << "'." << endl;
                return false;
            }
        }
    }
    return true;
}

static bool save_table_sections(const SaveOptions &options,
    const vector<pair<string, const Table*> > &sections)
{
    if (!options.enabled) return true;
    ofstream out(options.path.c_str());
    if (!out) {
        cerr << "Error: failed to open save output '" << options.path << "'." << endl;
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
    cout << "Saved " << (options.format == SAVE_FORMAT_CSV ? "csv" : "txt")
         << " output: " << options.path << endl;
    return true;
}

// static double get_time(struct timespec *start,
//     struct timespec *end)
// {
//     return end->tv_sec - start->tv_sec +
//         (end->tv_nsec - start->tv_nsec) * 1e-9;
// }

static void reg_new_isa(std::string isa,
    std::string type,
    std::string dim,
    int64_t loop_time,
    int64_t comp_pl,
    void *params,
    void (*bench)(int64_t, void*),
    int64_t inst_pl = 16)
{
    cpubm_t new_one;
    new_one.isa = isa;
    new_one.type = type;
    new_one.dim = dim;
    new_one.loop_time = loop_time;
    new_one.comp_pl = comp_pl;
    new_one.inst_pl = inst_pl;
    new_one.params = params;
    new_one.bench = bench;

    bm_list.push_back(new_one);
}

static void thread_func(void *params)
{
    cpubm_t *bm = (cpubm_t*)params;
    if (bm->params)
    {
        bm->bench(bm->loop_time, bm->params);
    }
    else
    {
        bm->bench(bm->loop_time, NULL);
    }
}
static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t*)params;
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    struct timespec start, end;
    cpubm_t warmup = item;
    warmup.loop_time = max<int64_t>(1, item.loop_time / LOOP_DECREASE);
    tpool_run_all(tm, thread_func, (void*)&warmup, &start, &end);

    ComputeResult result = {0.0, 0.0};
    if (!tpool_run_all(tm, thread_func, (void*)&item, &start, &end)) return result;
    double time_used = get_time(&start, &end);
    if (time_used <= 0.0) return result;
    result.perf = item.loop_time * item.comp_pl * tm->thread_num / time_used;
    if (!freq.empty() && freq[0] > 0.0)
        result.ipc = item.loop_time * item.inst_pl / time_used / freq[0] / 1e9;
    return result;
}

static string format_perf_value(double perf, const string &dim)
{
    char perfUnit = 'G';
    if (perf > 1e12)
    {
        perfUnit = 'T';
        perf /= 1e12;
    }
    else
    {
        perf /= 1e9;
    }

    stringstream ss;
    ss << std::setprecision(5) << perf << " " << perfUnit << dim;
    return ss.str();
}

static string format_latency_cycles(double latency)
{
    stringstream ss;
    ss << fixed << setprecision(2) << latency;
    return ss.str();
}

static void cpubm_x64_one(tpool_t *tm, cpubm_t &item, Table &table)
{
    ComputeResult result = cpubm_run_compute(tm, item);
    if (is_latency_benchmark(item)) {
        g_latency = result.ipc > 0.0 ? 1.0 / result.ipc : 0.0;
        return;
    }

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = format_perf_value(result.perf, item.dim);
    cont[3] = to_string(result.ipc);
    cont[4] = g_latency > 0.0 ? format_latency_cycles(g_latency) : "-";
    g_latency = 0.0;
    table.addOneItem(cont);
}

static void cpubm_x64_load(cpubm_t &item, Table &table)
{
    double perf = 0;

    vector<string> cont;
    cont.resize(table.getCol());

    double data_size = 0.0;
    bool is_l1 = item.isa == "L1 Cache" ||
        (item.isa == "--------" && item.comp_pl == 32LL);

    if (is_l1){
        data_size = cache_size.test_L1;
        cont[3] = to_string(cache_size.theory_L1) + " KB";
    } else {
        data_size = cache_size.test_L2;
        cont[3] = to_string(cache_size.theory_L2) + " KB";
    }

    perf = get_bandwith(item.loop_time, data_size, item.type);

    stringstream ss1;

    ss1 << setprecision(5) << perf << " " << item.dim;

    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss1.str();
    cont[4] = to_string(static_cast<int64_t>(data_size)) + " KB";
    table.addOneItem(cont);
}


static void cpubm_x64_cache(std::vector<int> &set_of_threads,Table &table)
{
    vector<string> cont;
    cont.resize(table.getCol());
    get_cacheline(&cache_size, set_of_threads[0]);
    // cout << "get cacheline" << endl;
    get_multiway(&cache_size, set_of_threads[0]);
    // cout << "get multiway" << endl;
    get_cachesize(&cache_size, set_of_threads[0]);
    if (cache_size.theory_L1 > 0 &&
        (cache_size.test_L1 <= 0 ||
         cache_size.test_L1 < cache_size.theory_L1 / 4 ||
         cache_size.test_L1 > cache_size.theory_L1 * 4))
        cache_size.test_L1 = cache_size.theory_L1;
    if (cache_size.theory_L2 > 0 &&
        (cache_size.test_L2 <= cache_size.test_L1 ||
         cache_size.test_L2 < cache_size.theory_L2 / 4 ||
         cache_size.test_L2 > cache_size.theory_L2 * 4))
        cache_size.test_L2 = cache_size.theory_L2;
    // cout << "get cachesize" << endl;
    cont[0] = "L1 ways of associativity";
    cont[1] = to_string(cache_size.theory_way);
    cont[2] = to_string(cache_size.test_way);
    table.addOneItem(cont);
    cont[0] = "cacheline size";
    cont[1] = to_string(cache_size.theory_cacheline) + " B";
    cont[2] = to_string(cache_size.test_cacheline) + " B";
    table.addOneItem(cont);
    cont[0] = "L1 cache size";
    cont[1] = to_string(cache_size.theory_L1) + " KB";
    cont[2] = to_string(cache_size.test_L1) + " KB";
    table.addOneItem(cont);
    cont[0] = "L2 cache size";
    cont[1] = to_string(cache_size.theory_L2) + " KB";
    cont[2] = to_string(cache_size.test_L2) + " KB";
    table.addOneItem(cont);
    return;
}

static void cpubm_x64_multiple_issue(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{

    struct timespec start, end;
    double time_used, perf;
    cache_bm_t bm;
    int size = 1024;
    float* cache_data = (float*)malloc(1024);
    //Preventing Compiler Optimization
    for (int i = 0;i < size / sizeof(float); i++){
        cache_data[i] = i;
    }
    int inner_loop = 1024;
    bm.bench = multiple_issue;
    if (item.type.find("ymm") != string::npos) bm.bench = multiple_issue_avx;
    else if (item.type.find("zmm") != string::npos) bm.bench = multiple_issue_avx512;
    bm.cache_data = cache_data;
    bm.inner_loop = inner_loop;
    bm.loop_time = item.loop_time;

	// warm up
    tpool_add_work(tm, cache_thread_func, (void*)&bm);
    tpool_wait(tm);

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    tpool_add_work(tm, cache_thread_func, (void*)&bm);
    tpool_wait(tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    perf = (double)item.loop_time * (inner_loop * item.comp_pl + 4)/
        (time_used * freq[0] * 1e9);
    stringstream ss;

    ss << setprecision(5) << perf;

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
    free(cache_data);
}

//comupte: Instruction Set / Core Computation / Peak Performance / IPC
//cachesize: cache level / Core Computation / bandwith / size / IPC / way
//frequency : core id / theory freq / test freq
//multi issue
static void init_table(vector<Table*> &tables)
{
    tables.resize(5);
    for (int  i = 0; i < 5; i++)
    {
        tables[i] = new Table();
    }

    vector<string> ti;

    ti.resize(5);
    ti[0] = "Instruction Set";
    ti[1] = "Core Computation";
    ti[2] = "Peak Performance";
    ti[3] = "Instr/TSC Cycle";
    ti[4] = "Latency(TSC cyc)";
    tables[0]->setColumnNum(ti.size());
    tables[0]->addOneItem(ti);

    ti.resize(5);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwidth";
    ti[3] = "Theory Size";
    ti[4] = "Test Size";
    tables[1]->setColumnNum(ti.size());
    tables[1]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Theory";
    ti[2] = "Test";
    tables[2]->setColumnNum(ti.size());
    tables[2]->addOneItem(ti);

    ti.resize(6);
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "TSC Freq";
    ti[3] = "Instr/TSC(FSU32)";
    ti[4] = "Instr/TSC(FSU64)";
    ti[5] = "Instr/TSC(LSU ldr)";
    tables[3]->setColumnNum(ti.size());
    tables[3]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Core Instruction";
    ti[2] = "Instr/TSC Cycle";
    tables[4]->setColumnNum(ti.size());
    tables[4]->addOneItem(ti);
}


static string format_thread_pool_prefix(const vector<int> &threads, size_t count)
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

static vector<int> find_compute_instruction_matches(const string &instruction)
{
    vector<int> exact_matches;
    vector<int> partial_matches;
    string needle = normalize_filter_value(instruction);
    for (int i = 0; i < static_cast<int>(bm_list.size()); ++i) {
        if (!is_compute_instruction_candidate(bm_list[i])) continue;
        string type = normalize_filter_value(bm_list[i].type);
        if (type == needle) exact_matches.push_back(i);
        else if (type.find(needle) != string::npos) partial_matches.push_back(i);
    }
    return exact_matches.empty() ? partial_matches : exact_matches;
}

static int find_latency_pair_index(const cpubm_t &item)
{
    string latency_type = item.type + "_latency";
    for (int i = 0; i < static_cast<int>(bm_list.size()); ++i) {
        if (bm_list[i].type == latency_type &&
            normalize_filter_value(bm_list[i].isa) == normalize_filter_value(item.isa))
            return i;
    }
    return -1;
}

static bool cpubm_do_instruction_sweep(vector<int> &set_of_threads,
    uint32_t idle_time, const string &instruction, const SaveOptions &save_options)
{
    vector<int> matches = find_compute_instruction_matches(instruction);
    if (matches.empty()) {
        cerr << "Error: no compute instruction matched '" << instruction << "'.\n"
             << "Use --list-instructions to list valid Core Computation names." << endl;
        return false;
    }
    if (matches.size() > 1) {
        cerr << "Error: instruction name '" << instruction
             << "' matched multiple compute instructions:" << endl;
        for (int idx : matches)
            cerr << "  " << bm_list[idx].isa << " / " << bm_list[idx].type << endl;
        return false;
    }

    cpubm_t selected = bm_list[matches[0]];
    int latency_index = find_latency_pair_index(selected);
    Table freq_table;
    vector<string> freq_head(6);
    freq_head[0] = "Core ID"; freq_head[1] = "Theory Freq";
    freq_head[2] = "TSC Freq"; freq_head[3] = "Instr/TSC(FSU32)";
    freq_head[4] = "Instr/TSC(FSU64)"; freq_head[5] = "Instr/TSC(LSU ldr)";
    freq_table.setColumnNum(freq_head.size());
    freq_table.addOneItem(freq_head);
    get_cpu_freq(set_of_threads, freq_table);

    Table table;
    vector<string> row(8);
    row[0] = "Cores"; row[1] = "Thread Pool"; row[2] = "Peak Performance";
    row[3] = "Peak/Core"; row[4] = "Speedup"; row[5] = "Efficiency";
    row[6] = "Instr/TSC Cycle"; row[7] = "Latency(TSC cyc)";
    table.setColumnNum(row.size());
    table.addOneItem(row);

    double baseline = 0.0;
    for (size_t cores = 1; cores <= set_of_threads.size(); ++cores) {
        vector<int> active(set_of_threads.begin(), set_of_threads.begin() + cores);
        tpool_t *tm = tpool_create(active);
        sleep(idle_time);
        string latency = "-";
        if (latency_index >= 0) {
            cpubm_t latency_item = bm_list[latency_index];
            ComputeResult latency_result = cpubm_run_compute(tm, latency_item);
            if (latency_result.ipc > 0.0)
                latency = format_latency_cycles(1.0 / latency_result.ipc);
        }
        ComputeResult result = cpubm_run_compute(tm, selected);
        if (cores == 1) baseline = result.perf;
        double speedup = baseline > 0.0 ? result.perf / baseline : 0.0;
        double efficiency = speedup / cores;
        row[0] = to_string(cores);
        row[1] = format_thread_pool_prefix(set_of_threads, cores);
        row[2] = format_perf_value(result.perf, selected.dim);
        row[3] = format_perf_value(result.perf / cores, selected.dim);
        { stringstream ss; ss << setprecision(4) << speedup << "x"; row[4] = ss.str(); }
        { stringstream ss; ss << setprecision(4) << efficiency * 100.0 << "%"; row[5] = ss.str(); }
        row[6] = to_string(result.ipc);
        row[7] = latency;
        table.addOneItem(row);
        tpool_destroy(tm);
    }
    table.print();
    vector<pair<string, const Table*> > sections;
    sections.push_back(make_pair(string("instruction_sweep"), &table));
    return save_table_sections(save_options, sections);
}

static bool cpubm_do_bench(vector<int> &set_of_threads, uint32_t idle_time,
    const BenchmarkFilter &filter, const SaveOptions &save_options)
{
    if (bm_list.empty()) return false;
    printf("Number Threads: %zu\nThread Pool Binding:", set_of_threads.size());
    for (size_t i = 0; i < set_of_threads.size(); ++i) printf(" %d", set_of_threads[i]);
    printf("\n");

    vector<Table*> tables;
    init_table(tables);
    if (benchmark_needs_freq(filter)) get_cpu_freq(set_of_threads, *tables[3]);
    if (should_run_test(filter, "cache"))
        cpubm_x64_cache(set_of_threads, *tables[2]);
    else if (should_run_test(filter, "load"))
        get_theory_cache(&cache_size, set_of_threads[0]);

    tpool_t *tm = tpool_create(set_of_threads);
    for (size_t i = 0; i < bm_list.size(); ++i) {
        if (!should_run_benchmark(filter, bm_list[i])) continue;
        sleep(idle_time);
        if (bm_list[i].dim.find("OPS") != string::npos)
            cpubm_x64_one(tm, bm_list[i], *tables[0]);
        else if (bm_list[i].dim.find("Byte/") != string::npos)
            cpubm_x64_load(bm_list[i], *tables[1]);
        else if (bm_list[i].dim.find("IPC") != string::npos)
            cpubm_x64_multiple_issue(tm, bm_list[i], *tables[4]);
    }

    vector<pair<string, const Table*> > sections;
    const char *names[] = {"compute", "load", "cache", "freq", "multi_issue"};
    for (int i = 0; i < 5; ++i) {
        if (should_run_test(filter, names[i])) {
            tables[i]->print();
            sections.push_back(make_pair(string(names[i]), tables[i]));
        }
    }
    bool ok = save_table_sections(save_options, sections);
    tpool_destroy(tm);
    for (size_t i = 0; i < tables.size(); ++i) delete tables[i];
    return ok;
}

static void cpufb_register_isa()
{
    bm_list.clear();
#ifdef _AMX_TILE_
    init_tile_cfg();
    syscall(SYS_arch_prctl, 0x1023, 18);
#endif

#ifdef _AVX2_
    reg_new_isa("AVX2", "ADD(s32,s32)_latency", "OPS",
        0x4000000LL, 128LL, NULL, avx2_add_s32_latency);
    reg_new_isa("AVX2", "ADD(s32,s32)", "OPS",
        0x4000000LL, 128LL, NULL, avx2_add_s32);
    reg_new_isa("AVX2", "MUL(s32,s32)_latency", "OPS",
        0x4000000LL, 128LL, NULL, avx2_mul_s32_latency);
    reg_new_isa("AVX2", "MUL(s32,s32)", "OPS",
        0x4000000LL, 128LL, NULL, avx2_mul_s32);
#endif

#ifdef _AVX512_IFMA_
    reg_new_isa("AVX512_IFMA", "MADD52(u64,u52,u52)_latency", "OPS",
        0x4000000LL, 256LL, NULL, avx512_ifma_madd52_u64_latency);
    reg_new_isa("AVX512_IFMA", "MADD52(u64,u52,u52)", "OPS",
        0x4000000LL, 256LL, NULL, avx512_ifma_madd52_u64);
#endif

#ifdef _AVX512_VBMI_
    reg_new_isa("AVX512_VBMI", "PERMB(u8)_latency", "OPS",
        0x4000000LL, 1024LL, NULL, avx512_vbmi_permb_u8_latency);
    reg_new_isa("AVX512_VBMI", "PERMB(u8)", "OPS",
        0x4000000LL, 1024LL, NULL, avx512_vbmi_permb_u8);
#endif

#ifdef _AVX512_VPOPCNTDQ_
    reg_new_isa("AVX512_VPOPCNTDQ", "POPCNT(s32)_latency", "OPS",
        0x4000000LL, 256LL, NULL, avx512_vpopcnt_s32_latency);
    reg_new_isa("AVX512_VPOPCNTDQ", "POPCNT(s32)", "OPS",
        0x4000000LL, 256LL, NULL, avx512_vpopcnt_s32);
#endif

#ifdef _AMX_INT8_
    reg_new_isa("AMX_INT8", "MM(s32,s8,s8)_latency", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32s8s8_latency, 4);
    reg_new_isa("AMX_INT8", "MM(s32,s8,s8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32s8s8, 4);
    reg_new_isa("AMX_INT8", "MM(s32,s8,u8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32s8u8, 4);
    reg_new_isa("AMX_INT8", "MM(s32,u8,s8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32u8s8, 4);
    reg_new_isa("AMX_INT8", "MM(s32,u8,u8)", "OPS",
        0x2500000LL, 131072LL, &__tilecfg, amx_int8_mm_s32u8u8, 4);
#endif

#ifdef _AMX_BF16_
    reg_new_isa("AMX_BF16", "MM(f32,bf16,bf16)_latency", "FLOPS",
        0x2500000LL, 65536LL, &__tilecfg, amx_bf16_mm_f32bf16bf16_latency, 4);
    reg_new_isa("AMX_BF16", "MM(f32,bf16,bf16)", "FLOPS",
        0x2500000LL, 65536LL, &__tilecfg, amx_bf16_mm_f32bf16bf16, 4);
#endif

#ifdef _AVX512_VNNI_
    reg_new_isa("AVX512_VNNI", "DP4A(s32,u8,s8)_latency", "OPS",
        0x4000000LL, 2048LL, NULL, avx512_vnni_dp4a_s32u8s8_latency);
    reg_new_isa("AVX512_VNNI", "DP4A(s32,u8,s8)", "OPS",
        0x20000000LL, 2048LL, NULL, avx512_vnni_dp4a_s32u8s8);
    reg_new_isa("AVX512_VNNI", "DP2A(s32,s16,s16)_latency", "OPS",
        0x4000000LL, 1024LL, NULL, avx512_vnni_dp2a_s32s16s16_latency);
    reg_new_isa("AVX512_VNNI", "DP2A(s32,s16,s16)", "OPS",
        0x20000000LL, 1024LL, NULL, avx512_vnni_dp2a_s32s16s16);
#endif

#ifdef _AVX_VNNI_
    reg_new_isa("AVX_VNNI", "DP4A(s32,u8,s8)_latency", "OPS",
        0x4000000LL, 1024LL, NULL, avx_vnni_dp4a_s32u8s8_latency);
    reg_new_isa("AVX_VNNI", "DP4A(s32,u8,s8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_dp4a_s32u8s8);
    reg_new_isa("AVX_VNNI", "DP2A(s32,s16,s16)_latency", "OPS",
        0x4000000LL, 512LL, NULL, avx_vnni_dp2a_s32s16s16_latency);
    reg_new_isa("AVX_VNNI", "DP2A(s32,s16,s16)", "OPS",
        0x20000000LL, 512LL, NULL, avx_vnni_dp2a_s32s16s16);
#endif

#ifdef _AVX_VNNI_INT8_
    reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,s8,s8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_int8_dp4a_s32s8s8);
    reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,s8,u8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_int8_dp4a_s32s8u8);
    reg_new_isa("AVX_VNNI_INT8", "DP4A(s32,u8,u8)", "OPS",
        0x20000000LL, 1024LL, NULL, avx_vnni_int8_dp4a_s32u8u8);
#endif

#ifdef _AVX512_BF16_
    reg_new_isa("AVX512_BF16", "DP2A(f32,bf16,bf16)_latency", "FLOPS",
        0x4000000LL, 1024LL, NULL, avx512_bf16_dp2a_f32bf16bf16_latency);
    reg_new_isa("AVX512_BF16", "DP2A(f32,bf16,bf16)", "FLOPS",
        0x20000000LL, 1024LL, NULL, avx512_bf16_dp2a_f32bf16bf16);
#endif

#ifdef _AVX512_FP16_
    reg_new_isa("AVX512_FP16", "FMA(f16,f16,f16)_latency", "FLOPS",
        0x4000000LL, 1024LL, NULL, avx512_fp16_fma_f16f16f16_latency);
    reg_new_isa("AVX512_FP16", "FMA(f16,f16,f16)", "FLOPS",
        0x20000000LL, 1024LL, NULL, avx512_fp16_fma_f16f16f16);
#endif

#ifdef _AVX512F_
    reg_new_isa("AVX512F", "FMA(f32,f32,f32)_latency", "FLOPS",
        0x4000000LL, 512LL, NULL, avx512f_fma_f32f32f32_latency);
    reg_new_isa("AVX512F", "FMA(f32,f32,f32)", "FLOPS",
        0x20000000LL, 512LL, NULL, avx512f_fma_f32f32f32);
    reg_new_isa("AVX512F", "FMA(f64,f64,f64)_latency", "FLOPS",
        0x4000000LL, 256LL, NULL, avx512f_fma_f64f64f64_latency);
    reg_new_isa("AVX512F", "FMA(f64,f64,f64)", "FLOPS",
        0x20000000LL, 256LL, NULL, avx512f_fma_f64f64f64);
#endif

#ifdef _FMA_
    reg_new_isa("FMA", "FMA(f32,f32,f32)_latency", "FLOPS",
        0x4000000LL, 256LL, NULL, fma_f32f32f32_latency);
    reg_new_isa("FMA", "FMA(f32,f32,f32)", "FLOPS",
        0x20000000LL, 256LL, NULL, fma_f32f32f32);
    reg_new_isa("FMA", "FMA(f64,f64,f64)_latency", "FLOPS",
        0x4000000LL, 128LL, NULL, fma_f64f64f64_latency);
    reg_new_isa("FMA", "FMA(f64,f64,f64)", "FLOPS",
        0x20000000LL, 128LL, NULL, fma_f64f64f64);
#endif

#ifdef _AVX_
    reg_new_isa("AVX", "ADD(f32,f32)_latency", "FLOPS", 0x4000000LL, 128LL, NULL, avx_add_f32_latency);
    reg_new_isa("AVX", "ADD(f32,f32)", "FLOPS", 0x4000000LL, 128LL, NULL, avx_add_f32);
    reg_new_isa("AVX", "MUL(f32,f32)_latency", "FLOPS", 0x4000000LL, 128LL, NULL, avx_mul_f32_latency);
    reg_new_isa("AVX", "MUL(f32,f32)", "FLOPS", 0x4000000LL, 128LL, NULL, avx_mul_f32);
    reg_new_isa("AVX", "ADD(f64,f64)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, avx_add_f64_latency);
    reg_new_isa("AVX", "ADD(f64,f64)", "FLOPS", 0x4000000LL, 64LL, NULL, avx_add_f64);
    reg_new_isa("AVX", "MUL(f64,f64)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, avx_mul_f64_latency);
    reg_new_isa("AVX", "MUL(f64,f64)", "FLOPS", 0x4000000LL, 64LL, NULL, avx_mul_f64);
    reg_new_isa("AVX", "ADD(MUL(f32,f32),f32)", "FLOPS",
        0x20000000LL, 128LL, NULL, avx_add_mul_f32f32_f32);
    reg_new_isa("AVX", "ADD(MUL(f64,f64),f64)", "FLOPS",
        0x20000000LL, 64LL, NULL, avx_add_mul_f64f64_f64);
#endif

#ifdef _SSE_
    reg_new_isa("SSE", "ADD(f32,f32)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, sse_add_f32_latency);
    reg_new_isa("SSE", "ADD(f32,f32)", "FLOPS", 0x4000000LL, 64LL, NULL, sse_add_f32);
    reg_new_isa("SSE", "MUL(f32,f32)_latency", "FLOPS", 0x4000000LL, 64LL, NULL, sse_mul_f32_latency);
    reg_new_isa("SSE", "MUL(f32,f32)", "FLOPS", 0x4000000LL, 64LL, NULL, sse_mul_f32);
    reg_new_isa("SSE", "ADD(MUL(f32,f32),f32)", "FLOPS",
        0x20000000LL, 64LL, NULL, sse_add_mul_f32f32_f32);
#endif

#ifdef _SSE2_
    reg_new_isa("SSE2", "ADD(f64,f64)_latency", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_add_f64_latency);
    reg_new_isa("SSE2", "ADD(f64,f64)", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_add_f64);
    reg_new_isa("SSE2", "MUL(f64,f64)_latency", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_mul_f64_latency);
    reg_new_isa("SSE2", "MUL(f64,f64)", "FLOPS", 0x4000000LL, 32LL, NULL, sse2_mul_f64);
    reg_new_isa("SSE2", "ADD(MUL(f64,f64),f64)", "FLOPS",
        0x20000000LL, 32LL, NULL, sse2_add_mul_f64f64_f64);
#endif

    reg_new_isa("L1 Cache", "vmovups.ymm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
    reg_new_isa("--------", "movss.scalar(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
    reg_new_isa("--------", "movups.xmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
#ifdef _AVX512F_
    reg_new_isa("--------", "vmovups.zmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 32LL, NULL, NULL);
#endif
    reg_new_isa("L2 Cache", "vmovups.ymm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
    reg_new_isa("--------", "movss.scalar(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
    reg_new_isa("--------", "movups.xmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
#ifdef _AVX512F_
    reg_new_isa("--------", "vmovups.zmm(f32)", "Byte/TSC Cycle",
        0x186A00LL, 128LL, NULL, NULL);
#endif
    reg_new_isa("MULTI_ISSUE", "ldr/fmla", "IPC",
        0x40000LL, 34LL, NULL, NULL);
#ifdef _FMA_
    reg_new_isa("MULTI_ISSUE_AVX", "vmovups/vfmadd.ymm", "IPC",
        0x40000LL, 34LL, NULL, NULL);
#endif
#ifdef _AVX512F_
    reg_new_isa("MULTI_ISSUE_AVX512", "vmovups/vfmadd.zmm", "IPC",
        0x40000LL, 34LL, NULL, NULL);
#endif
}

int main(int argc, char *argv[])
{
    vector<int> set_of_threads;
    uint32_t idle_time = 0;
    BenchmarkFilter filter;
    bool list_categories = false;
    bool list_instructions = false;
    string sweep_instruction;
    SaveOptions save_options;
    bool params_enough = false;

    int i;
    for (i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--thread_pool=", 14) == 0)
        {
            parse_thread_pool(argv[i] + 14, set_of_threads);
            params_enough = true;
        }
        else if (strncmp(argv[i], "--idle_time=", 12) == 0)
        {
            idle_time = (uint32_t)atoi(argv[i] + 12);
        }
        else if (strncmp(argv[i], "--include-isa=", 14) == 0)
            parse_filter_list(argv[i] + 14, filter.include_isa);
        else if (strncmp(argv[i], "--exclude-isa=", 14) == 0)
            parse_filter_list(argv[i] + 14, filter.exclude_isa);
        else if (strncmp(argv[i], "--include-test=", 15) == 0)
            parse_filter_list(argv[i] + 15, filter.include_test);
        else if (strncmp(argv[i], "--exclude-test=", 15) == 0)
            parse_filter_list(argv[i] + 15, filter.exclude_test);
        else if (strcmp(argv[i], "--list-categories") == 0)
            list_categories = true;
        else if (strcmp(argv[i], "--list-instructions") == 0)
            list_instructions = true;
        else if (strncmp(argv[i], "--sweep-instruction=", 20) == 0)
            sweep_instruction = argv[i] + 20;
        else if (strncmp(argv[i], "--scale-instruction=", 20) == 0)
            sweep_instruction = argv[i] + 20;
        else if (strncmp(argv[i], "--save=", 7) == 0) {
            save_options.enabled = true;
            save_options.path = argv[i] + 7;
        }
        else if (strncmp(argv[i], "--output=", 9) == 0) {
            save_options.enabled = true;
            save_options.path = argv[i] + 9;
        }
        else if (strncmp(argv[i], "--save-format=", 14) == 0) {
            if (!parse_save_format(argv[i] + 14, save_options.format)) return 1;
            save_options.format_set = true;
        }
        else if (strncmp(argv[i], "--output-format=", 16) == 0) {
            if (!parse_save_format(argv[i] + 16, save_options.format)) return 1;
            save_options.format_set = true;
        }
    }
    if (list_categories || list_instructions) {
        cpufb_register_isa();
        if (list_categories) print_benchmark_categories();
        if (list_instructions) print_benchmark_instructions();
        return 0;
    }
    if (!params_enough)
    {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --idle_time parameter.\n");
        fprintf(stderr, "Usage: %s --thread_pool=[xxx] [--idle_time=yyy] [--include-test=list] [--exclude-test=list] [--include-isa=list] [--exclude-isa=list]\n", argv[0]);
        fprintf(stderr, "       %s --thread_pool=[xxx] --sweep-instruction='Core Computation'\n", argv[0]);
        fprintf(stderr, "       %s --list-categories | --list-instructions\n", argv[0]);
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr, "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        return 1;
    }

    if (set_of_threads.empty()) {
        fprintf(stderr, "Error: --thread_pool must contain at least one CPU.\n");
        return 1;
    }
    if (save_options.enabled) {
        save_options.path = trim_arg_value(save_options.path);
        if (save_options.path.empty()) return 1;
        if (!save_options.format_set)
            save_options.format = infer_save_format_from_path(save_options.path);
    }
    cpufb_register_isa();
    if (!validate_benchmark_filter(filter)) return 1;
    if (!sweep_instruction.empty())
        return cpubm_do_instruction_sweep(set_of_threads, idle_time,
            sweep_instruction, save_options) ? 0 : 1;
    return cpubm_do_bench(set_of_threads, idle_time, filter, save_options) ? 0 : 1;
}
