#include <unistd.h>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdint>
#include <vector>
#include <set>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <utility>

#include <stdlib.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "table.hpp"
#include "thread_pool.hpp"
#include<load.hpp>
#include<compute.hpp>
#include<frequency.hpp>
#include<multiple_issue.hpp>
#include<common.hpp>
#include <cmath>

#if defined(__linux__) && !defined(__APPLE__)
#include "runtime_features.hpp"
#endif

#ifdef __APPLE__
#include<amx.hpp>
#endif

using namespace std;
extern vector<double> freq;
static struct CacheData cache_size;
static int64_t load_pl = 0;
static int64_t g_latency = 0;
typedef struct
{
    string isa;
    string type;
    string dim;
    int64_t loop_time;
    int64_t comp_pl;
    void*  bench;
    const char* required_feature;
} cpubm_t;

typedef struct
{
    float* cache_data;
    int inner_loop;
    int loop_time;
    void (*bench)(float*, int, int64_t);
} cache_bm_t;

static vector<cpubm_t> bm_list;
static const char* registration_required_feature = "_ASIMD_";

static void require_feature(const char* feature)
{
    registration_required_feature = feature;
}

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

    SaveOptions() :
        enabled(false),
        format_set(false),
        format(SAVE_FORMAT_TXT)
    {
    }
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

static bool ends_with_case_insensitive(const string &value,
    const string &suffix)
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
    if (ends_with_case_insensitive(path, ".csv")) return SAVE_FORMAT_CSV;
    return SAVE_FORMAT_TXT;
}

static const char *save_format_name(SaveFormat format)
{
    return format == SAVE_FORMAT_CSV ? "csv" : "txt";
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
    const set<string> &exclude,
    const string &value)
{
    if (exclude.find(value) != exclude.end()) return false;
    return include.empty() || include.find(value) != include.end();
}

static string get_benchmark_test_type(const cpubm_t &item)
{
    if (item.dim.find("Byte/Cycle") != string::npos) return "load";
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
        return filter_allows_value(filter.include_isa,
            filter.exclude_isa,
            normalize_filter_value(item.isa));
    }

    return true;
}

static bool benchmark_needs_freq(const BenchmarkFilter &filter)
{
    return should_run_test(filter, "compute") ||
        should_run_test(filter, "load") ||
        should_run_test(filter, "multi_issue") ||
        should_run_test(filter, "freq");
}

static bool should_run_standalone_warmup(const BenchmarkFilter &filter)
{
    return filter.include_test.size() == 1 || !filter.include_isa.empty();
}

static bool is_latency_benchmark(const cpubm_t &item)
{
    return item.type.find("_latency") != string::npos;
}

static bool is_compute_instruction_candidate(const cpubm_t &item)
{
    return get_benchmark_test_type(item) == "compute" &&
        !is_latency_benchmark(item);
}

static void print_benchmark_categories()
{
    set<string> isa_categories;

    for (const cpubm_t &item : bm_list) {
        if (get_benchmark_test_type(item) == "compute") {
            isa_categories.insert(item.isa);
        }
    }

    cout << "Test categories:" << endl;
    cout << "  compute" << endl;
    cout << "  load" << endl;
    cout << "  cache" << endl;
    cout << "  freq" << endl;
    cout << "  multi_issue" << endl;

    cout << "ISA categories:" << endl;
    for (const string &isa : isa_categories) {
        cout << "  " << isa << endl;
    }
}

static void print_benchmark_instructions()
{
    Table table;
    vector<string> row;

    row.resize(3);
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

static bool save_table_sections(const SaveOptions &save_options,
    const vector<pair<string, const Table*> > &sections)
{
    if (!save_options.enabled) return true;

    ofstream out(save_options.path.c_str());
    if (!out) {
        cerr << "Error: failed to open save output '" << save_options.path
             << "'." << endl;
        return false;
    }

    if (save_options.format == SAVE_FORMAT_CSV) {
        for (size_t i = 0; i < sections.size(); i++) {
            sections[i].second->writeCompact(out, ',', sections[i].first);
        }
    } else {
        for (size_t i = 0; i < sections.size(); i++) {
            if (i != 0) out << '\n';
            out << "[" << sections[i].first << "]\n";
            sections[i].second->writeCompact(out, '\t');
        }
    }

    cout << "Saved " << save_format_name(save_options.format)
         << " output: " << save_options.path << endl;
    return true;
}

static void reg_new_isa(string isa,
    string type,
    string dim,
    int64_t loop_time,
    int64_t comp_pl,
    void* bench)
{
    cpubm_t new_one;
    new_one.isa = isa;
    new_one.type = type;
    new_one.dim = dim;
    new_one.loop_time = loop_time;
    new_one.comp_pl = comp_pl;
    new_one.bench = (void *)bench;
    new_one.required_feature = registration_required_feature;

    bm_list.push_back(new_one);
}
static void thread_func(void *params)
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#endif
    cpubm_t *bm = (cpubm_t*)params;
    ((void(*)(int64_t))bm->bench)(bm->loop_time);
}

static void cache_thread_func(void *params)
{
    cache_bm_t *bm = (cache_bm_t*)params;
    bm->bench(bm->cache_data, bm->inner_loop, bm->loop_time);
}

static void cpubm_standalone_warmup(vector<int> &set_of_threads)
{
    for (const cpubm_t &item : bm_list) {
        if (get_benchmark_test_type(item) != "compute") continue;

        cpubm_t warmup_item = item;
        warmup_item.loop_time = min<int64_t>(warmup_item.loop_time, 0x4000LL);

        tpool_t *tm = tpool_create(set_of_threads);
        for (int i = 0; i < tm->thread_num; i++) {
            tpool_add_work(tm, thread_func, (void*)&warmup_item);
        }
        tpool_wait(tm);
        tpool_destroy(tm);
        return;
    }
}

static double cpubm_measure_compute_time(tpool_t *tm, cpubm_t &item)
{
    struct timespec start, end;

#ifdef __APPLE__
    constexpr int kBenchRepeats = 5;
#else
    constexpr int kBenchRepeats = 5;
    constexpr double kTargetSeconds = 0.05;
    constexpr int64_t kMaxLoopScale = 1024;
#endif
    double best_time = 1e30;
#ifndef __APPLE__
    // Calibrate each instruction to a long enough measurement window. The
    // old fixed loop count made high-throughput 32-core kernels last only a
    // few milliseconds, so thread wake-up skew dominated the result.
    cpubm_t run_item = item;
    if (!tpool_run_all(tm, thread_func, (void*)&run_item, &start, &end))
        return best_time;
    double probe_time = get_time(&start, &end);
    int64_t loop_scale = 1;
    if (probe_time > 0 && probe_time < kTargetSeconds) {
        loop_scale = static_cast<int64_t>(ceil(kTargetSeconds / probe_time));
        loop_scale = max<int64_t>(1, min<int64_t>(loop_scale, kMaxLoopScale));
    }
    if (item.loop_time > INT64_MAX / loop_scale)
        loop_scale = INT64_MAX / item.loop_time;
    run_item.loop_time = item.loop_time * loop_scale;

    for (int rep = 0; rep < kBenchRepeats; ++rep) {
        if (!tpool_run_all(tm, thread_func, (void*)&run_item, &start, &end))
            return best_time;
        // Normalize to the registered loop count so existing FLOP/OP
        // accounting remains unchanged.
        double t = get_time(&start, &end) / loop_scale;
        if (t < best_time) best_time = t;
    }
#else
     // warm up
    //pthread_set_qos_class_self_np( QOS_CLASS_UTILITY, 0 );
    for (int i = 0; i < tm->thread_num; ++i) {
        // ((void(*)(int64_t))item.bench)(item.loop_time);
        dispatch_group_async(tm->group, tm->queue, ^{((void(*)(int64_t))item.bench)(item.loop_time);});
    }
    dispatch_group_wait(tm->group, DISPATCH_TIME_FOREVER);
    for (int rep = 0; rep < kBenchRepeats; ++rep) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        for (int j = 0; j < tm->thread_num; ++j) {
            // ((void(*)(int64_t))item.bench)(item.loop_time);
            dispatch_group_async(tm->group, tm->queue, ^{((void(*)(int64_t))item.bench)(item.loop_time);});
        }
        dispatch_group_wait(tm->group, DISPATCH_TIME_FOREVER);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        double t = get_time(&start, &end);
        if (t < best_time) best_time = t;
    }
#endif

    return best_time;
}

static int64_t cpubm_scaled_comp_pl(const cpubm_t &item)
{
    int64_t comp_pl = item.comp_pl;
#ifdef _SVE_
    if (item.type.find("sve") != string::npos) {
        comp_pl = comp_pl * load_sve_vector_bytes();
    }
#endif
#ifdef _SME_
    const string &type = item.type;
    bool has_opa = type.find("opa.vv") != string::npos;
    string first_param = has_opa ? type.substr(type.find('('), type.find(',')) : "";

    if (has_opa && first_param.find("32") != string::npos) {
        comp_pl = comp_pl * rdsvl() * rdsvl() / 4 / 4;
        //cout << type << " op = " << item.comp_pl << endl;
    } else if (has_opa && first_param.find("64") != string::npos) {
        comp_pl = comp_pl * rdsvl() * rdsvl() / 8 / 8;
        //cout << type << " op = " << item.comp_pl << endl;
    } else if (has_opa && first_param.find("16") != string::npos) {
        // 16-bit accumulator (SME_F16F16): rows=cols=SVL/2.
        comp_pl = comp_pl * rdsvl() * rdsvl() / 2 / 2;
    } else if (type.find("sme") != string::npos) {
        comp_pl = comp_pl * rdsvl();
        //cout << type << " is sme " << item.comp_pl <<endl;
    }
#endif

    return comp_pl;
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
    ss << setprecision(5) << perf << " " << perfUnit << dim;
    return ss.str();
}

static ComputeResult cpubm_run_compute(tpool_t *tm, cpubm_t &item)
{
    ComputeResult result;
    double time_used = cpubm_measure_compute_time(tm, item);
    int64_t comp_pl = cpubm_scaled_comp_pl(item);

    result.perf = item.loop_time * comp_pl * tm->thread_num / time_used;
    // time_used is the synchronized wall time for one invocation per core.
    // Report estimated IPC per core rather than summing all cores into an
    // aggregate value that grows with the thread count.
    result.ipc = item.loop_time * 24 / time_used / freq[0] / 1e9;
    return result;
}

static void cpubm_arm64_one(tpool_t *tm,
    cpubm_t &item, Table &table)
{
    // cout << "test fop begin" << endl;
    ComputeResult result = cpubm_run_compute(tm, item);
    string perf = format_perf_value(result.perf, item.dim);

    if (item.type.find("latency") != string::npos) {
        g_latency = round(1 / result.ipc);
    } else {
        vector<string> cont;
        cont.resize(table.getCol());
        // cout << "table size = " << table.getCol() << endl;
        cont[0] = item.isa;
        cont[1] = item.type;
        cont[2] = perf;
        cont[3] = to_string(result.ipc);
        cont[4] = g_latency != 0 ? to_string(g_latency) : "-";
        g_latency = 0;
        table.addOneItem(cont);
    }

    // cout << "test fop end" << endl;
}

static void cpubm_arm_load(tpool_t *tm, cpubm_t &item, Table &table)
{
    double perf = 0;

    vector<string> cont;
    cont.resize(table.getCol());
    //cout << "test load begin" << endl;

    if (item.isa == "L1 Cache"){
        load_pl = cache_size.theory_L1 > 0 ? cache_size.theory_L1 : cache_size.test_L1;
        cont[3] = to_string(cache_size.theory_L1) + " KB";
        cont[4] = to_string(cache_size.test_L1) + " KB";
    } else if (item.isa == "L2 Cache"){
        load_pl = cache_size.theory_L2 > 0 ? cache_size.theory_L2 : cache_size.test_L2;
        cont[3] = to_string(cache_size.theory_L2) + " KB";
        cont[4] = to_string(cache_size.test_L2) + " KB";
    } 

    tpool_t *load_tm = (tm != nullptr && tm->thread_num > 1) ? tm : nullptr;
    perf = get_bandwith(item.loop_time, (double)load_pl, item.type, item.bench, load_tm);

    stringstream ss1;

    ss1 << setprecision(5) << perf << " " << item.dim;

    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss1.str();
   
    table.addOneItem(cont);
    //cout << "test load end" << endl;
}

static void sanitize_cache_size_probe()
{
#ifdef __linux__
    if (cache_size.theory_L1 > 0 &&
        (cache_size.test_L1 <= 0 ||
         cache_size.test_L1 < cache_size.theory_L1 / 4 ||
         cache_size.test_L1 > cache_size.theory_L1 * 4)) {
        cache_size.test_L1 = cache_size.theory_L1;
    }

    if (cache_size.theory_L2 > 0 &&
        (cache_size.test_L2 <= cache_size.test_L1 ||
         cache_size.test_L2 < cache_size.theory_L2 / 4 ||
         cache_size.test_L2 > cache_size.theory_L2 * 4)) {
        cache_size.test_L2 = cache_size.theory_L2;
    }
#endif
}

static void probe_arm_cache(std::vector<int> &set_of_threads)
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np( QOS_CLASS_USER_INTERACTIVE, 0 );
#endif

    get_cacheline(&cache_size, set_of_threads[0]);
    // cout << "get cacheline" << endl;
    get_multiway(&cache_size, set_of_threads[0]);
    // cout << "get multiway" << endl;
    get_cachesize(&cache_size, set_of_threads[0]);
    sanitize_cache_size_probe();
    // cout << "get cachesize" << endl;
}

static void cpubm_arm_cache(std::vector<int> &set_of_threads,Table &table)
{
    vector<string> cont;

    cont.resize(table.getCol());
    probe_arm_cache(set_of_threads);
    cont[0] = "L1 ways of associativity";
    cont[1] = to_string(cache_size.theory_way);
    cont[2] = to_string(cache_size.test_way);
    table.addOneItem(cont);
    cont[0] = "cacheline size";
    cont[1] = to_string(cache_size.theory_cacheline) + " B";
    cont[2] = to_string(cache_size.test_cacheline) + " B";
    table.addOneItem(cont);
    return;
}


static void cpubm_arm_multiple_issue(tpool_t *tm,
    cpubm_t &item,
    Table &table)
{
    //cout << "test multi issue start" << endl;
    struct timespec start, end;
    double time_used, perf;
    cache_bm_t bm;
    int num_threads = tm->thread_num;
    int size = 2048;
    float* cache_data = (float*)malloc(size);
    //Preventing Compiler Optimization
    for (int i = 0;i < size / sizeof(float); i++){
        cache_data[i] = i;
    }
    int inner_loop = 1024;
    // if (item.type.find("sme")) {
    bm.bench = reinterpret_cast<void (*)(float *, int, int64_t)>(item.bench);
    // } else {
    //     bm.bench = multiple_issue;
    // }   
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

    ss << setprecision(5) << perf << " " << item.dim;

    vector<string> cont;
    cont.resize(table.getCol());
    cont[0] = item.isa;
    cont[1] = item.type;
    cont[2] = ss.str();
    table.addOneItem(cont);
    free(cache_data);
    //cout << "test multi issue end" << endl;
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
    ti[3] = "IPC";
    ti[4] = "Latency";
    tables[0]->setColumnNum(ti.size());
    tables[0]->addOneItem(ti);

    ti.resize(6);
    ti[0] = "Cache Level";
    ti[1] = "Core Instruction";
    ti[2] = "Bandwith";
    ti[3] = "Theory Size";
    ti[4] = "Test Size";
    ti[5] = "Latency";
    tables[1]->setColumnNum(ti.size());
    tables[1]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Theory";
    ti[2] = "Test";
    tables[2]->setColumnNum(ti.size());
    tables[2]->addOneItem(ti);

    #ifdef _SVE_
    ti.resize(8);
    #else
    ti.resize(6);
    #endif
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "Test Freq";
    ti[3] = "IPC(FSU32)";
    ti[4] = "IPC(FSU64)";
    ti[5] = "IPC(LSU ldr)";
    #ifdef _SVE_
    ti[6] = "IPC(SVE32)";
    ti[7] = "IPC(SVE64)";
    #endif
    tables[3]->setColumnNum(ti.size());
    tables[3]->addOneItem(ti);

    ti.resize(3);
    ti[0] = "Item";
    ti[1] = "Core Instruction";
    ti[2] = "IPC";
    tables[4]->setColumnNum(ti.size());
    tables[4]->addOneItem(ti);
}

static void init_frequency_table(Table &table)
{
    vector<string> ti;

#ifdef _SVE_
    ti.resize(8);
#else
    ti.resize(6);
#endif
    ti[0] = "Core ID";
    ti[1] = "Theory Freq";
    ti[2] = "Test Freq";
    ti[3] = "IPC(FSU32)";
    ti[4] = "IPC(FSU64)";
    ti[5] = "IPC(LSU ldr)";
#ifdef _SVE_
    ti[6] = "IPC(SVE32)";
    ti[7] = "IPC(SVE64)";
#endif
    table.setColumnNum(ti.size());
    table.addOneItem(ti);
}

static string format_thread_pool_prefix(const vector<int> &set_of_threads,
    size_t count)
{
    stringstream ss;

    ss << "[";
    for (size_t i = 0; i < count; i++) {
        if (i != 0) ss << ",";
        ss << set_of_threads[i];
    }
    ss << "]";

    return ss.str();
}

static string format_ratio_value(double value)
{
    stringstream ss;

    ss << setprecision(4) << value << "x";
    return ss.str();
}

static string format_percent_value(double value)
{
    stringstream ss;

    ss << setprecision(4) << value * 100.0 << "%";
    return ss.str();
}

static vector<int> find_compute_instruction_matches(const string &instruction)
{
    vector<int> exact_matches;
    vector<int> partial_matches;
    string needle = normalize_filter_value(instruction);

    if (needle.empty()) return exact_matches;

    for (int i = 0; i < static_cast<int>(bm_list.size()); i++) {
        const cpubm_t &item = bm_list[i];
        if (!is_compute_instruction_candidate(item)) continue;

        string type = normalize_filter_value(item.type);
        if (type == needle) {
            exact_matches.push_back(i);
        } else if (type.find(needle) != string::npos) {
            partial_matches.push_back(i);
        }
    }

    return exact_matches.empty() ? partial_matches : exact_matches;
}

static int find_latency_pair_index(const cpubm_t &item)
{
    string latency_type = item.type + "_latency";
    string item_isa = normalize_filter_value(item.isa);

    for (int i = 0; i < static_cast<int>(bm_list.size()); i++) {
        const cpubm_t &candidate = bm_list[i];
        if (candidate.type == latency_type &&
            normalize_filter_value(candidate.isa) == item_isa) {
            return i;
        }
    }

    for (int i = 0; i < static_cast<int>(bm_list.size()); i++) {
        if (bm_list[i].type == latency_type) return i;
    }

    return -1;
}

static bool cpubm_do_instruction_sweep(vector<int> &set_of_threads,
    uint32_t idle_time,
    const string &instruction,
    const SaveOptions &save_options)
{
    if (bm_list.empty()) {
        printf("Sorry, there's no any supported SIMD isa.\n");
        return false;
    }

    vector<int> matches = find_compute_instruction_matches(instruction);
    if (matches.empty()) {
        cerr << "Error: no compute instruction matched '" << instruction << "'." << endl;
        cerr << "Use --list-instructions to list valid Core Computation names." << endl;
        return false;
    }
    if (matches.size() > 1) {
        cerr << "Error: instruction name '" << instruction
             << "' matched multiple compute instructions:" << endl;
        for (int idx : matches) {
            cerr << "  " << bm_list[idx].isa << " / " << bm_list[idx].type << endl;
        }
        cerr << "Use the full Core Computation name to select one instruction." << endl;
        return false;
    }

    cpubm_t selected_item = bm_list[matches[0]];
    int latency_index = find_latency_pair_index(selected_item);

    int num_threads = set_of_threads.size();
    printf("Instruction Sweep: %s / %s\n",
        selected_item.isa.c_str(), selected_item.type.c_str());
    printf("Thread Pool Binding:");
    for (int i = 0; i < num_threads; i++) {
        printf(" %d", set_of_threads[i]);
    }
    printf("\n");
#ifdef _SVE_
    if (arm64_runtime_features().sve)
        cout << " SVE : " << load_sve_vector_bytes() << endl;
#endif
#ifdef _SME_
    if (arm64_runtime_features().sme)
        cout << "SME : " << rdsvl() * 8 << endl;
#endif

    Table freq_table;
    init_frequency_table(freq_table);
    get_cpu_freq(set_of_threads, freq_table);

    Table table;
    vector<string> row;
    row.resize(8);
    row[0] = "Cores";
    row[1] = "Thread Pool";
    row[2] = "Peak Performance";
    row[3] = "Peak/Core";
    row[4] = "Speedup";
    row[5] = "Efficiency";
    row[6] = "IPC";
    row[7] = "Latency";
    table.setColumnNum(row.size());
    table.addOneItem(row);

    double baseline_perf = 0.0;

    for (int cores = 1; cores <= num_threads; cores++) {
        vector<int> active_threads(set_of_threads.begin(),
            set_of_threads.begin() + cores);
        tpool_t *tm = tpool_create(active_threads);
        string latency = "-";

        sleep(idle_time);

        if (latency_index >= 0) {
            cpubm_t latency_item = bm_list[latency_index];
            ComputeResult latency_result = cpubm_run_compute(tm, latency_item);
            if (latency_result.ipc > 0) {
                latency = to_string(static_cast<int64_t>(
                    round(1 / latency_result.ipc)));
            }
        }

        cpubm_t run_item = selected_item;
        ComputeResult result = cpubm_run_compute(tm, run_item);
        if (cores == 1) baseline_perf = result.perf;

        double speedup = baseline_perf > 0 ? result.perf / baseline_perf : 0.0;
        double efficiency = cores > 0 ? speedup / cores : 0.0;

        row[0] = to_string(cores);
        row[1] = format_thread_pool_prefix(set_of_threads, cores);
        row[2] = format_perf_value(result.perf, selected_item.dim);
        row[3] = format_perf_value(result.perf / cores, selected_item.dim);
        row[4] = format_ratio_value(speedup);
        row[5] = format_percent_value(efficiency);
        row[6] = to_string(result.ipc);
        row[7] = latency;
        table.addOneItem(row);

        tpool_destroy(tm);
    }

    table.print();

    if (save_options.enabled) {
        Table metadata;
        vector<string> metadata_row;
        metadata_row.resize(2);
        metadata_row[0] = "Item";
        metadata_row[1] = "Value";
        metadata.setColumnNum(metadata_row.size());
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Instruction Set";
        metadata_row[1] = selected_item.isa;
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Core Computation";
        metadata_row[1] = selected_item.type;
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Metric";
        metadata_row[1] = selected_item.dim;
        metadata.addOneItem(metadata_row);
        metadata_row[0] = "Thread Pool";
        metadata_row[1] = format_thread_pool_prefix(set_of_threads,
            set_of_threads.size());
        metadata.addOneItem(metadata_row);

        vector<pair<string, const Table*> > sections;
        sections.push_back(make_pair(string("sweep_metadata"), &metadata));
        sections.push_back(make_pair(string("instruction_sweep"), &table));
        if (!save_table_sections(save_options, sections)) return false;
    }

    return true;
}

static bool cpubm_do_bench(vector<int> &set_of_threads,
    uint32_t idle_time,
    const BenchmarkFilter &filter,
    const SaveOptions &save_options)
{
    int i;

    if (bm_list.size() > 0)
    {
        int num_threads = set_of_threads.size();

        printf("Number Threads: %d\n", num_threads);
        printf("Thread Pool Binding:");
        for (i = 0; i < num_threads; i++)
        {
            printf(" %d", set_of_threads[i]);
        }
        printf("\n");
#ifdef _SVE_
        if (arm64_runtime_features().sve)
            cout << " SVE : " << load_sve_vector_bytes() << endl;
#endif

#ifdef _SME_
        if (arm64_runtime_features().sme)
            cout << "SME : " << rdsvl() * 8 << endl;
#endif
        // set table head
        vector<Table*> tables;
        init_table(tables);
        // cout << "start benchmark" << endl;
        if (should_run_standalone_warmup(filter)) {
            cpubm_standalone_warmup(set_of_threads);
        }
        if (benchmark_needs_freq(filter)) {
            get_cpu_freq(set_of_threads, *tables[3]);
        }
        // exit(0);
        // cout << "get freq" << endl;
        if (should_run_test(filter, "cache")) {
            cpubm_arm_cache(set_of_threads, *tables[2]);
        } else if (should_run_test(filter, "load")) {
            probe_arm_cache(set_of_threads);
        }
        // set thread pool
        tpool_t *tm;
        tm = tpool_create(set_of_threads);

        // traverse task list
        for (i = 1; i < bm_list.size(); i++)
        { 
            // cout << bm_list[i].type << endl;
            if (!should_run_benchmark(filter, bm_list[i])) continue;

            sleep(idle_time);
            if (bm_list[i].dim.find("OPS") != string::npos) {
                cpubm_arm64_one(tm, bm_list[i], *tables[0]);
            } else if (bm_list[i].dim.find("Byte/Cycle") != string::npos) {
                cpubm_arm_load(tm, bm_list[i], *tables[1]);
            } else if (bm_list[i].dim.find("IPC") != string::npos) {
                cpubm_arm_multiple_issue(tm, bm_list[i], *tables[4]);
            } else {
                cout << "Wrong dimension !" << endl;
                break;
            }
        }
        vector<pair<string, const Table*> > save_sections;
        if (should_run_test(filter, "compute")) {
            tables[0]->print();
            save_sections.push_back(make_pair(string("compute"), tables[0]));
        }
        if (should_run_test(filter, "load")) {
            tables[1]->print();
            save_sections.push_back(make_pair(string("load"), tables[1]));
        }
        if (should_run_test(filter, "cache")) {
            tables[2]->print();
            save_sections.push_back(make_pair(string("cache"), tables[2]));
        }
        if (should_run_test(filter, "freq")) {
            tables[3]->print();
            save_sections.push_back(make_pair(string("freq"), tables[3]));
        }
        if (should_run_test(filter, "multi_issue")) {
            tables[4]->print();
            save_sections.push_back(make_pair(string("multi_issue"), tables[4]));
        }
        bool save_ok = save_table_sections(save_options, save_sections);
        tpool_destroy(tm);
        return save_ok;
    }
    else
    {
        printf("Sorry, there's no any supported SIMD isa.\n");
        return false;
    }
}

static void cpufb_register_isa()
{
    bm_list.clear();
    require_feature("_ASIMD_");
#ifdef __APPLE__
    constexpr int64_t kComputeLoopTime = 0x40000LL;
    constexpr int64_t kLatencyLoopTime = 0x4000LL;
    constexpr int64_t kLoadLoopTime = 0x40000LL;
    constexpr int64_t kMultiIssueLoopTime = 0x40000LL;
#else
    constexpr int64_t kComputeLoopTime = 0x100000LL;
    constexpr int64_t kLatencyLoopTime = 0x10000LL;
    constexpr int64_t kLoadLoopTime = 0x186A00LL;
    constexpr int64_t kMultiIssueLoopTime = 0x186A00LL;
#endif

#ifdef _I8MM_
    require_feature("_I8MM_");
    reg_new_isa("i8mm", "mmla(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 1536LL, (void*)asimd_mmla_s32s8s8_latency);
    reg_new_isa("i8mm", "mmla(s32,s8,s8)", "OPS",
        kComputeLoopTime, 1536LL, (void*)asimd_mmla_s32s8s8);
    reg_new_isa("i8mm", "mmla(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 1536LL, (void*)asimd_mmla_u32u8u8_latency);
    reg_new_isa("i8mm", "mmla(u32,u8,u8)", "OPS",
        kComputeLoopTime, 1536LL, (void*)asimd_mmla_u32u8u8);
    reg_new_isa("i8mm", "mmla(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 1536LL, (void*)asimd_mmla_s32u8s8_latency);
    reg_new_isa("i8mm", "mmla(s32,u8,s8)", "OPS",
        kComputeLoopTime, 1536LL, (void*)asimd_mmla_s32u8s8);

    reg_new_isa("i8mm", "dp4a.vs(s32,s8,u8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8u8_latency);
    reg_new_isa("i8mm", "dp4a.vs(s32,s8,u8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8u8);
    reg_new_isa("i8mm", "dp4a.vs(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_s32u8s8_latency);
    reg_new_isa("i8mm", "dp4a.vs(s32,u8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_s32u8s8);
    reg_new_isa("i8mm", "dp4a.vv(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vv_s32u8s8_latency);
    reg_new_isa("i8mm", "dp4a.vv(s32,u8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vv_s32u8s8);
#endif

#ifdef _ASIMD_DP_
    require_feature("_ASIMD_DP_");
    reg_new_isa("asimd_dp", "dp4a.vs(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8s8_latency);
    reg_new_isa("asimd_dp", "dp4a.vs(s32,s8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_s32s8s8);
    reg_new_isa("asimd_dp", "dp4a.vv(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vv_s32s8s8_latency);
    reg_new_isa("asimd_dp", "dp4a.vv(s32,s8,s8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vv_s32s8s8);
    reg_new_isa("asimd_dp", "dp4a.vs(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vs_u32u8u8_latency);
    reg_new_isa("asimd_dp", "dp4a.vs(u32,u8,u8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vs_u32u8u8);
    reg_new_isa("asimd_dp", "dp4a.vv(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 768LL, (void*)asimd_dp4a_vv_u32u8u8_latency);
    reg_new_isa("asimd_dp", "dp4a.vv(u32,u8,u8)", "OPS",
        kComputeLoopTime, 768LL, (void*)asimd_dp4a_vv_u32u8u8);
#endif

#ifdef _BF16_
    require_feature("_BF16_");
    reg_new_isa("bf16", "mmla(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 768LL, (void*)asimd_mmla_fp32bf16bf16_latency);
    reg_new_isa("bf16", "mmla(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 768LL, (void*)asimd_mmla_fp32bf16bf16);
    reg_new_isa("bf16", "dp2a.vs(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_dp2a_vs_fp32bf16bf16_latency);
    reg_new_isa("bf16", "dp2a.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_dp2a_vs_fp32bf16bf16);
    reg_new_isa("bf16", "dp2a.vv(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_dp2a_vv_fp32bf16bf16_latency);
    reg_new_isa("bf16", "dp2a.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_dp2a_vv_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalb(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalb_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalb(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalb_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalt(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalt_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalt(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalt_fp32bf16bf16);
    // By-element (lane / laneq) variants. FLOPS coefficient matches the
    // vector form: 24 instr * 4 dest lanes * 2 (FMA) = 192.
    reg_new_isa("bf16", "bfmlalb.lane(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalb_lane_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalb.lane(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalb_lane_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalt.lane(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalt_lane_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalt.lane(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalt_lane_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalb.laneq(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalb_laneq_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalb.laneq(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalb_laneq_fp32bf16bf16);
    reg_new_isa("bf16", "bfmlalt.laneq(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_bfmlalt_laneq_fp32bf16bf16_latency);
    reg_new_isa("bf16", "bfmlalt.laneq(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_bfmlalt_laneq_fp32bf16bf16);
#endif

#ifdef _FHM_
    require_feature("_FHM_");
    reg_new_isa("FHM", "fmlal.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal_vv_f32f16f16);
    reg_new_isa("FHM", "fmlal.vv(f32,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmlal_vv_f32f16f16_latency);
    reg_new_isa("FHM", "fmlal2.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal2_vv_f32f16f16);
    reg_new_isa("FHM", "fmlal2.vv(f32,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmlal2_vv_f32f16f16_latency);
    reg_new_isa("FHM", "fmlal.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal_vs_f32f16f16);
    reg_new_isa("FHM", "fmlal_pair.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmlal_pair_vv_f32f16f16);
#endif

#ifdef _ASIMD_HP_
    require_feature("_ASIMD_HP_");
    reg_new_isa("asimd_hp", "fmla.vs(fp16,fp16,fp16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fmla_vs_fp16fp16fp16);
    reg_new_isa("asimd_hp", "fmla.vv(fp16,fp16,fp16)", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fmla_vv_fp16fp16fp16);
#endif

#ifdef _ASIMD_
    require_feature("_ASIMD_");
    reg_new_isa("asimd", "fmla.vs(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmla2_vs_f32f32f32);
    reg_new_isa("asimd", "fmla.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmla_vs_f32f32f32);
    reg_new_isa("asimd", "fmla.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmla2_vv_f32f32f32);
    reg_new_isa("asimd", "fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "fmla.vs(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fmla2_vs_f64f64f64);
    reg_new_isa("asimd", "fmla.vs(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmla_vs_f64f64f64);
    reg_new_isa("asimd", "fmla.vv(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fmla2_vv_f64f64f64);
    reg_new_isa("asimd", "fmla.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmla_vv_f64f64f64);
    reg_new_isa("asimd", "hybrid_fp32_mla_6x16", "FLOPS",
        kComputeLoopTime, 768LL, (void*)asimd_hybrid_fp32_mla_6x16);
    // Tier-2 additions: non-FMA paths and negated-FMA chain.
    reg_new_isa("asimd", "fmls.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fmls_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fmls.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmls_vv_f32f32f32);
    reg_new_isa("asimd", "fneg+fmla.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fneg_fmla_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fneg+fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fneg_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "fadd.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fadd_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fadd.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fadd_vv_f32f32f32);
    reg_new_isa("asimd", "fmul.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_fmul_vv_f32f32f32_latency);
    reg_new_isa("asimd", "fmul.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmul_vv_f32f32f32);
#endif
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("asimd", "sve_fmla.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fmla_vs_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fmla2_vv_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fmla_vv_f32f32f32);
    reg_new_isa("asimd", "sve_fmla.vs(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fmla_vs_f64f64f64);
    reg_new_isa("asimd", "sve_fmla.vv(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fmla2_vv_f64f64f64);
    reg_new_isa("asimd", "sve_fmla.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fmla_vv_f64f64f64);
    // Tier-2 additions: SVE complex FCMLA + reductions.
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#0_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_0_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#0", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_0);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#90_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_90_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#90", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_90);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#180_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_180_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#180", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_180);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#270_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_270_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f32,f32,f32)#270", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fcmla_vv_f32f32f32_270);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#0_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_0_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#0", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_0);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#90_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_90_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#90", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_90);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#180_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_180_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#180", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_180);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#270_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_270_latency);
    reg_new_isa("sve_complex", "sve_fcmla.vv(f64,f64,f64)#270", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_fcmla_vv_f64f64f64_270);
    // sve_fadda: Pattern D — same kernel registered twice (latency probe +
    // displayed throughput row) so the displayed row carries fadda's own
    // latency rather than leaking it onto the unrelated row that follows.
    reg_new_isa("sve_reduce", "sve_fadda(f32)_latency", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fadda_v_f32);
    reg_new_isa("sve_reduce", "sve_fadda(f32)", "FLOPS",
        kLatencyLoopTime, 6LL, (void*)sve_fadda_v_f32);
    reg_new_isa("sve_reduce", "sve_fadda(f64)_latency", "FLOPS",
        kLatencyLoopTime, 3LL, (void*)sve_fadda_v_f64);
    reg_new_isa("sve_reduce", "sve_fadda(f64)", "FLOPS",
        kLatencyLoopTime, 3LL, (void*)sve_fadda_v_f64);
    reg_new_isa("sve_reduce", "sve_faddv(f32)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sve_faddv_v_f32);
    reg_new_isa("sve_reduce", "sve_faddv(f64)", "FLOPS",
        kComputeLoopTime, 3LL, (void*)sve_faddv_v_f64);
#endif

#ifdef _ASIMD_FCMA_
    require_feature("_ASIMD_FCMA_");
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#0_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_0_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#0", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_0);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#90_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_90_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#90", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_90);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#180_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_180_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#180", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_180);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#270_latency", "FLOPS",
        kLatencyLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_270_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f32,f32,f32)#270", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_vv_f32f32f32_270);
    reg_new_isa("asimd_fcma", "fcmla_pair.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)asimd_fcmla_pair_vv_f32f32f32);
  #ifdef _ASIMD_HP_
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#0_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_0_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#0", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_0);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#90_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_90_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#90", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_90);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#180_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_180_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#180", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_180);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#270_latency", "FLOPS",
        kLatencyLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_270_latency);
    reg_new_isa("asimd_fcma", "fcmla.vv(f16,f16,f16)#270", "FLOPS",
        kComputeLoopTime, 384LL, (void*)asimd_fcmla_vv_f16f16f16_270);
  #endif
#endif

#ifdef _ASIMD_REDUCE_
    require_feature("_ASIMD_REDUCE_");
    reg_new_isa("asimd_reduce", "faddp.vvv(f32)_latency", "FLOPS",
        kLatencyLoopTime, 96LL, (void*)asimd_faddp_v_f32_latency);
    reg_new_isa("asimd_reduce", "faddp.vvv(f32)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)asimd_faddp_v_f32);
    reg_new_isa("asimd_reduce", "fmaxv(f32)", "OPS",
        kComputeLoopTime, 96LL, (void*)asimd_fmaxv_v_f32);
    reg_new_isa("asimd_reduce", "saddlv(s8)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_saddlv_v_s8);
    reg_new_isa("asimd_reduce", "smaxv(s32)", "OPS",
        kComputeLoopTime, 96LL, (void*)asimd_smaxv_v_s32);
  #ifdef _ASIMD_HP_
    reg_new_isa("asimd_reduce", "fmaxv(f16)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_fmaxv_v_f16);
  #endif
#endif

#ifdef _ASIMD_RECIP_
    require_feature("_ASIMD_RECIP_");
    reg_new_isa("asimd_recip", "frecpe+frecps(f32)_latency", "FLOPS",
        kLatencyLoopTime, 144LL, (void*)asimd_frecpe_recps_v_f32_latency);
    reg_new_isa("asimd_recip", "frecpe+frecps(f32)", "FLOPS",
        kComputeLoopTime, 144LL, (void*)asimd_frecpe_recps_v_f32);
    reg_new_isa("asimd_recip", "frsqrte+frsqrts(f32)_latency", "FLOPS",
        kLatencyLoopTime, 144LL, (void*)asimd_frsqrte_rsqrts_v_f32_latency);
    reg_new_isa("asimd_recip", "frsqrte+frsqrts(f32)", "FLOPS",
        kComputeLoopTime, 144LL, (void*)asimd_frsqrte_rsqrts_v_f32);
  #ifdef _ASIMD_HP_
    reg_new_isa("asimd_recip", "frecpe+frecps(f16)_latency", "FLOPS",
        kLatencyLoopTime, 288LL, (void*)asimd_frecpe_recps_v_f16_latency);
    reg_new_isa("asimd_recip", "frecpe+frecps(f16)", "FLOPS",
        kComputeLoopTime, 288LL, (void*)asimd_frecpe_recps_v_f16);
  #endif
#endif

#ifdef _ASIMD_INT_MAC_
    require_feature("_ASIMD_INT_MAC_");
    reg_new_isa("asimd_int_mac", "mla.vs(s32,s32,s32)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_mla_vs_s32s32s32_latency);
    reg_new_isa("asimd_int_mac", "mla.vs(s32,s32,s32)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_mla_vs_s32s32s32);
    reg_new_isa("asimd_int_mac", "mla.vv(s32,s32,s32)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_mla_vv_s32s32s32_latency);
    reg_new_isa("asimd_int_mac", "mla.vv(s32,s32,s32)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_mla_vv_s32s32s32);
    reg_new_isa("asimd_int_mac", "mla.vs(s16,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 384LL, (void*)asimd_mla_vs_s16s16s16_latency);
    reg_new_isa("asimd_int_mac", "mla.vs(s16,s16,s16)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_mla_vs_s16s16s16);
    reg_new_isa("asimd_int_mac", "sqdmlal.vv(s32,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_sqdmlal_vv_s32s16s16_latency);
    reg_new_isa("asimd_int_mac", "sqdmlal.vv(s32,s16,s16)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_sqdmlal_vv_s32s16s16);
    reg_new_isa("asimd_int_mac", "sqdmlal2.vv(s32,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 192LL, (void*)asimd_sqdmlal2_vv_s32s16s16_latency);
    reg_new_isa("asimd_int_mac", "sqdmlal2.vv(s32,s16,s16)", "OPS",
        kComputeLoopTime, 192LL, (void*)asimd_sqdmlal2_vv_s32s16s16);
#endif

#ifdef _ASIMD_TBL_
    require_feature("_ASIMD_TBL_");
    // tbl/tbx do 16 lookups per .16b instr, so the OPS number doubles as
    // Byte/Cycle (16 lookup-bytes/instr * IPC). The dispatcher routes any
    // "Byte/Cycle" dim into cpubm_arm_load (different kernel ABI), so we
    // only register the OPS form here; convert mentally as needed.
    reg_new_isa("asimd_tbl", "tbl.4table(u8)_latency", "OPS",
        kLatencyLoopTime, 384LL, (void*)asimd_tbl_4table_v_u8_latency);
    reg_new_isa("asimd_tbl", "tbl.4table(u8)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_tbl_4table_v_u8);
    reg_new_isa("asimd_tbl", "tbx.4table(u8)_latency", "OPS",
        kLatencyLoopTime, 384LL, (void*)asimd_tbx_4table_v_u8_latency);
    reg_new_isa("asimd_tbl", "tbx.4table(u8)", "OPS",
        kComputeLoopTime, 384LL, (void*)asimd_tbx_4table_v_u8);
#endif

#ifdef _SVE_I8MM_
    require_feature("_SVE_I8MM_");
    reg_new_isa("sve_i8mm", "sve_mmla(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sve_mmla_s32s8s8_latency);
    reg_new_isa("sve_i8mm", "sve_mmla(s32,s8,s8)", "OPS",
        kComputeLoopTime, 96LL, (void*)sve_mmla_s32s8s8);
    reg_new_isa("sve_i8mm", "sve_mmla(u32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sve_mmla_u32u8u8_latency);
    reg_new_isa("sve_i8mm", "sve_mmla(u32,u8,u8)", "OPS",
        kComputeLoopTime, 96LL, (void*)sve_mmla_u32u8u8);
    reg_new_isa("sve_i8mm", "sve_mmla(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sve_mmla_s32u8s8_latency);
    reg_new_isa("sve_i8mm", "sve_mmla(s32,u8,s8)", "OPS",
        kComputeLoopTime, 96LL, (void*)sve_mmla_s32u8s8);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,s8,s8)_latency", "OPS",
        kLatencyLoopTime, 12LL, (void*)sve_dp4a_vv_s32s8s8_latency);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,s8,s8)", "OPS",
        kComputeLoopTime, 12LL, (void*)sve_dp4a_vv_s32s8s8);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,u8)_latency", "OPS",
        kLatencyLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8u8_latency);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,u8)", "OPS",
        kComputeLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8u8);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,s8)_latency", "OPS",
        kLatencyLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8s8_latency);
    reg_new_isa("sve_i8mm", "sve_dp4a.vv(s32,u8,s8)", "OPS",
        kComputeLoopTime, 12LL, (void*)sve_dp4a_vv_s32u8s8);
#endif

#ifdef _SVE_BF16_
    require_feature("_SVE_BF16_");
    reg_new_isa("sve_bf16", "sve_bfmmla(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sve_bfmmla_f32bf16bf16_latency);
    reg_new_isa("sve_bf16", "sve_bfmmla(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sve_bfmmla_f32bf16bf16);
    reg_new_isa("sve_bf16", "sve_bfdot.vv(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 24LL, (void*)sve_bfdot_vv_f32bf16bf16_latency);
    reg_new_isa("sve_bf16", "sve_bfdot.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_bfdot_vv_f32bf16bf16);
    reg_new_isa("sve_bf16", "sve_bfdot.vs(f32,bf16,bf16)_latency", "FLOPS",
        kLatencyLoopTime, 24LL, (void*)sve_bfdot_vs_f32bf16bf16_latency);
    reg_new_isa("sve_bf16", "sve_bfdot.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_bfdot_vs_f32bf16bf16);
#endif

#ifdef _SVE_F32MM_
    require_feature("_SVE_F32MM_");
    // 24 inst * 32 FLOPs/inst (at VL=128b) / 16 (svcntb scaling unit) = 48
    reg_new_isa("sve_f32mm", "sve_fmmla(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sve_fmmla_f32f32f32);
    reg_new_isa("sve_f32mm", "sve_fmmla(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sve_fmmla_f32f32f32_latency);
#endif

#ifdef _SVE_F64MM_
    require_feature("_SVE_F64MM_");
    // 24 inst * 8 FLOPs/inst (at VL=128b) / 16 = 12
    reg_new_isa("sve_f64mm", "sve_fmmla(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sve_fmmla_f64f64f64);
    reg_new_isa("sve_f64mm", "sve_fmmla(f64,f64,f64)_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sve_fmmla_f64f64f64_latency);
#endif

#ifdef _SVE_FP16_FMLA_
    require_feature("_SVE_FP16_FMLA_");
    // 24 inst * 16 FLOPs/inst (at VL=128b, 8 fp16 lanes * 2 ops) / 16 = 24
    reg_new_isa("sve_fp16", "sve_fmla.vv(f16,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_fmla_vv_f16f16f16);
    reg_new_isa("sve_fp16", "sve_fmla.vs(f16,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sve_fmla_vs_f16f16f16);
    reg_new_isa("sve_fp16", "sve_fmla.vv(f16,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 24LL, (void*)sve_fmla_vv_f16f16f16_latency);
#endif

#ifdef _SVE2_
    require_feature("_SVE2_");
    // 24 inst * 16 OPs/inst (at VL=128b, 8 i16 lanes * 2 ops) / 16 = 24
    reg_new_isa("sve2", "sve2_sqrdmlah.vv(s16,s16,s16)", "OPS",
        kComputeLoopTime, 24LL, (void*)sve2_sqrdmlah_vv_s16s16s16);
    reg_new_isa("sve2", "sve2_sqrdmlah.vv(s16,s16,s16)_latency", "OPS",
        kLatencyLoopTime, 24LL, (void*)sve2_sqrdmlah_vv_s16s16s16_latency);
#endif

#ifdef _SME_
    require_feature("_SME_B16F32_");
    reg_new_isa("SME", "sme_bfmopa.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme_bfmopa_vv_f32bf16bf16);
    require_feature("_SME_F32F32_");
    reg_new_isa("SME", "sme_fmopa.vv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sme_fmopa2_vv_f32f32f32);
    reg_new_isa("SME", "sme_fmopa.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa_vv_f32f32f32);
    require_feature("_SME_F16F32_");
    reg_new_isa("SME", "sme_fmopa.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme_fmopa_vv_f32f16f16);
    require_feature("_SME_I8I32_");
    reg_new_isa("SME", "sme_smopa.vv(i32,i8,i8)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)sme_smopa_vv_i32i8i8);
    reg_new_isa("SME", "sme_umopa.vv(u32,u8,u8)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)sme_umopa_vv_u32u8u8);
    reg_new_isa("SME", "sme_usmopa.vv(s32,u8,s8)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)sme_usmopa_vv_s32u8s8);
    reg_new_isa("SME", "sme_sumopa.vv(s32,s8,u8)", "FLOPS",
        kComputeLoopTime, 192LL, (void*)sme_sumopa_vv_s32s8u8);
    require_feature("_SME_F32F32_");
    reg_new_isa("SME_MULTI_ISSUE", "ldr/fmopa", "IPC",
        0x186A0LL, 40LL, (void*)sme_multiple_issue);
#endif

#ifdef _SME_F16F16_
    require_feature("_SME_F16F16_");
    // f16 acc: rows=cols=SVL/2, per inst = SVL^2/2 FLOPs.
    // Scaling rule (16 branch): comp_pl * SVL^2 / 4. Register 48 (=24*2).
    reg_new_isa("SME_F16F16", "sme_fmopa.vv(f16,f16,f16)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa_vv_f16f16f16);
    reg_new_isa("SME_F16F16", "sme_fmopa.vv(f16,f16,f16)_latency", "FLOPS",
        kLatencyLoopTime, 48LL, (void*)sme_fmopa2_vv_f16f16f16);
#endif

#ifdef _SME_I16I32_
    require_feature("_SME_I16I32_");
    // i32 acc, i16 elems: rows=cols=SVL/4, per inst = SVL^2/4 OPs.
    // Scaling rule (32 branch): comp_pl * SVL^2 / 16. Register 96 (=24*4).
    reg_new_isa("SME_I16I32", "sme_smopa.vv(i32,i16,i16)", "OPS",
        kComputeLoopTime, 96LL, (void*)sme_smopa_vv_i32i16i16);
    reg_new_isa("SME_I16I32", "sme_smopa.vv(i32,i16,i16)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sme_smopa2_vv_i32i16i16);
    reg_new_isa("SME_I16I32", "sme_umopa.vv(i32,i16,i16)", "OPS",
        kComputeLoopTime, 96LL, (void*)sme_umopa_vv_i32i16i16);
    reg_new_isa("SME_I16I32", "sme_umopa.vv(i32,i16,i16)_latency", "OPS",
        kLatencyLoopTime, 96LL, (void*)sme_umopa2_vv_i32i16i16);
#endif

#ifdef _SME2_
    require_feature("_SME2_");
    reg_new_isa("SME2", "sme2_bfmlal.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfmlal_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfmlal4_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfmlal_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfmlal4_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfmlal_mvv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfmlal4.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfmlal4_mvv_f32bf16bf16);

    reg_new_isa("SME2", "sme2_bfdot.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfdot_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.vs(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfdot4_vs_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfdot_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.vv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfdot4_vv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_bfdot_mvv_f32bf16bf16);
    reg_new_isa("SME2", "sme2_bfdot4.mvv(f32,bf16,bf16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_bfdot4_mvv_f32bf16bf16);
    
    reg_new_isa("SME2", "sme2_fmla.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sme2_fmla_vs_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.vs(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla4_vs_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sme2_fmla_vv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.vv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla4_vv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.mvv(f32,f32,f32)_latency", "FLOPS",
        kLatencyLoopTime, 12LL, (void*)sme2_fmla2_mvv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla.mvv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 12LL, (void*)sme2_fmla_mvv_f32f32f32);
    reg_new_isa("SME2", "sme2_fmla4.mvv(f32,f32,f32)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme2_fmla4_mvv_f32f32f32);

    reg_new_isa("SME2", "sme2_fmlal.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmlal_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal4_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmlal_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal4_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmlal_mvv_f32f16f16);
    reg_new_isa("SME2", "sme2_fmlal4.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fmlal4_mvv_f32f16f16);
    
    reg_new_isa("SME2", "sme2_fvdot.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 36LL, (void*)sme2_fvdot_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fvdot2.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 72LL, (void*)sme2_fvdot2_vs_f32f16f16);
    
    reg_new_isa("SME2", "sme2_fdot.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fdot_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot4.vs(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot4_vs_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fdot_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot4.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot4_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot.vv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fdot_vv_f32f16f16);
    reg_new_isa("SME2", "sme2_fdot4.mvv(f32,f16,f16)", "FLOPS",
        kComputeLoopTime, 96LL, (void*)sme2_fdot4_mvv_f32f16f16);
#endif


#ifdef _SMEf64_
    require_feature("_SME_F64F64_");
    reg_new_isa("SMEf64", "sme_fmopa2.vv(f64,f64,f64)_latency", "FLOPS",
    kComputeLoopTime, 48LL, (void*)sme_fmopa2_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme_fmopa.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 48LL, (void*)sme_fmopa_vv_f64f64f64); 
    require_feature("_SME2_F64F64_");
    reg_new_isa("SMEf64", "sme2_fmla.vs(f64,f64,f64)_latency", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla2_vs_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.vs(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla_vs_f64f64f64);
    
    reg_new_isa("SMEf64", "sme2_fmla4.vs(f64,f64,f64)", "FLOPS",

        kComputeLoopTime, 24LL, (void*)sme2_fmla4_vs_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.vv(f64,f64,f64)_latency", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla2_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla4.vv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla4_vv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla.mvv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 6LL, (void*)sme2_fmla_mvv_f64f64f64);
    reg_new_isa("SMEf64", "sme2_fmla4.mvv(f64,f64,f64)", "FLOPS",
        kComputeLoopTime, 24LL, (void*)sme2_fmla4_mvv_f64f64f64);
#endif
    require_feature("_LDP_");
    reg_new_isa("L1 Cache", "ldp(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_ldp_kernel);
    reg_new_isa("--------", "neon-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1b_kernel);
    reg_new_isa("--------", "neon-ld1h-x4(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1h_kernel);
    reg_new_isa("--------", "neon-ld1h-4x1-post(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1h_4x1_post_kernel);
    reg_new_isa("--------", "neon-ld1h-4x1-ptr(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1h_4x1_ptr_kernel);
    reg_new_isa("--------", "ldrq-4x1-off(128b)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_ldrq_4x1_offset_kernel);
    reg_new_isa("--------", "neon-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1w_kernel);
    reg_new_isa("--------", "neon-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_neon_ld1d_kernel);
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("--------", "sve-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_sve_ld1b_kernel);
    reg_new_isa("--------", "sve-ld1h(f16)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_sve_ld1h_kernel);
    reg_new_isa("--------", "sve-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_ld1w_kernel);
    reg_new_isa("--------", "sve-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)load_sve_ld1d_kernel);
#endif
#ifdef _SME_
    require_feature("_SME_");
    reg_new_isa("--------", "ldrZA(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ldr_kernel);
    reg_new_isa("--------", "ld1wZAV(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wV_kernel);
    reg_new_isa("--------", "ld1wZAH(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wH_kernel);
#endif
#ifdef _SME2_
    require_feature("_SME2_");
    reg_new_isa("--------", "ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1w_kernel);
#endif
    require_feature("_LDP_");
    reg_new_isa("L2 Cache", "ldp(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_ldp_kernel);
    reg_new_isa("--------", "neon-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1b_kernel);
    reg_new_isa("--------", "neon-ld1h-x4(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1h_kernel);
    reg_new_isa("--------", "neon-ld1h-4x1-post(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1h_4x1_post_kernel);
    reg_new_isa("--------", "neon-ld1h-4x1-ptr(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1h_4x1_ptr_kernel);
    reg_new_isa("--------", "ldrq-4x1-off(128b)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_ldrq_4x1_offset_kernel);
    reg_new_isa("--------", "neon-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1w_kernel);
    reg_new_isa("--------", "neon-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_neon_ld1d_kernel);
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("--------", "sve-ld1b(u8)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_sve_ld1b_kernel);
    reg_new_isa("--------", "sve-ld1h(f16)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_sve_ld1h_kernel);
    reg_new_isa("--------", "sve-ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_ld1w_kernel);
    reg_new_isa("--------", "sve-ld1d(f64)", "Byte/Cycle",
        kLoadLoopTime, 128LL, (void*)load_sve_ld1d_kernel);
#endif
#ifdef _SME_
    require_feature("_SME_");
    reg_new_isa("--------", "ldrZA(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ldr_kernel);   
    reg_new_isa("--------", "ld1wZAV(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wV_kernel);
    reg_new_isa("--------", "ld1wZAH(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1wH_kernel);     
#endif
#ifdef _SME2_
    require_feature("_SME2_");
    reg_new_isa("--------", "ld1w(f32)", "Byte/Cycle",
        kLoadLoopTime, 32LL, (void*)sme_ld1w_kernel);
#endif
#ifdef __APPLE__
    // reg_new_isa("Apple amx", "fmla.mat(f16,f16,f16)", "FLOPS",
    //     kComputeLoopTime, 16384LL, (void*)fmla16_benchmark_mat);
    // reg_new_isa("Apple amx", "fmla.mat(f32,f32,f32)", "FLOPS",
    //     kComputeLoopTime, 4096LL, (void*)fmla32_benchmark_mat);
    // reg_new_isa("Apple amx", "fmla.mat(f64,f64,f64)", "FLOPS",
    //     kComputeLoopTime, 1024LL, (void*)fmla64_benchmark_mat);

    // reg_new_isa("Apple amx", "fmla.vec(f16,f16,f16)", "FLOPS",
    //     kComputeLoopTime, 512LL, (void*)fmla16_benchmark_vec);
    // reg_new_isa("Apple amx", "fmla.vec(f32,f32,f32)", "FLOPS",
    //     kComputeLoopTime, 256LL, (void*)fmla32_benchmark_vec);
    // reg_new_isa("Apple amx", "fmla.vec(f64,f64,f64)", "FLOPS",
    //     kComputeLoopTime, 128LL, (void*)fmla64_benchmark_vec);

    // reg_new_isa("Apple amx", "mat.mat(i8,i8,i8)", "OPS",
    //     kComputeLoopTime, 65536LL, (void*)matint_i8i8_benchmark);
    // reg_new_isa("Apple amx", "fmla.mat(i8,i16,i16)", "OPS",
    //     kComputeLoopTime, 32768LL, (void*)matint_i8i16_benchmark);
    // reg_new_isa("Apple amx", "fmla.mat(i16,i16,i16)", "OPS",
    //     kComputeLoopTime, 16384LL, (void*)matint_i16i16_benchmark);

    // reg_new_isa("Apple amx", "ldx 1 reg", "Byte/Cycle",
    //     kLoadLoopTime, 32LL, (void*)load_benchmark_1);   
    // reg_new_isa("Apple amx", "ldx 2 reg", "Byte/Cycle",
    //     kLoadLoopTime, 32LL, (void*)load_benchmark_2);
    // reg_new_isa("Apple amx", "ldx 4 reg", "Byte/Cycle",
    //     kLoadLoopTime, 32LL, (void*)load_benchmark_4);    
    
    
#endif
#ifdef _SVE_
    require_feature("_SVE_");
    reg_new_isa("SVE_MULTI_ISSUE", "ld1w/fmla", "IPC",
        kMultiIssueLoopTime, 34LL, (void*)sve_multiple_issue);
#endif
    require_feature("_ISSUE_");
    reg_new_isa("MULTI_ISSUE", "ldr/fmla", "IPC",
        kMultiIssueLoopTime, 50LL, (void*)multiple_issue);

#if defined(__linux__) && !defined(__APPLE__)
    const Arm64RuntimeFeatures &features = arm64_runtime_features();
    bm_list.erase(remove_if(bm_list.begin(), bm_list.end(),
        [&](const cpubm_t &item) {
            return !features.supports(item.required_feature);
        }),
        bm_list.end());

    cout << "Runtime ARM64 ISA:";
    for (const string &token : features.runnable_tokens()) cout << ' ' << token;
    cout << endl;
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
        {
            parse_filter_list(argv[i] + 14, filter.include_isa);
        }
        else if (strncmp(argv[i], "--exclude-isa=", 14) == 0)
        {
            parse_filter_list(argv[i] + 14, filter.exclude_isa);
        }
        else if (strncmp(argv[i], "--include-test=", 15) == 0)
        {
            parse_filter_list(argv[i] + 15, filter.include_test);
        }
        else if (strncmp(argv[i], "--exclude-test=", 15) == 0)
        {
            parse_filter_list(argv[i] + 15, filter.exclude_test);
        }
        else if (strcmp(argv[i], "--list-categories") == 0)
        {
            list_categories = true;
        }
        else if (strcmp(argv[i], "--list-instructions") == 0)
        {
            list_instructions = true;
        }
        else if (strncmp(argv[i], "--sweep-instruction=", 20) == 0)
        {
            sweep_instruction = argv[i] + 20;
        }
        else if (strncmp(argv[i], "--scale-instruction=", 20) == 0)
        {
            sweep_instruction = argv[i] + 20;
        }
        else if (strncmp(argv[i], "--save=", 7) == 0)
        {
            save_options.enabled = true;
            save_options.path = argv[i] + 7;
        }
        else if (strncmp(argv[i], "--output=", 9) == 0)
        {
            save_options.enabled = true;
            save_options.path = argv[i] + 9;
        }
        else if (strncmp(argv[i], "--save-format=", 14) == 0)
        {
            if (!parse_save_format(argv[i] + 14, save_options.format)) {
                fprintf(stderr, "Error: unsupported --save-format value '%s'. Use txt or csv.\n",
                    argv[i] + 14);
                return 1;
            }
            save_options.format_set = true;
        }
        else if (strncmp(argv[i], "--output-format=", 16) == 0)
        {
            if (!parse_save_format(argv[i] + 16, save_options.format)) {
                fprintf(stderr, "Error: unsupported --output-format value '%s'. Use txt or csv.\n",
                    argv[i] + 16);
                return 1;
            }
            save_options.format_set = true;
        }
    }

    if (list_categories || list_instructions)
    {
        cpufb_register_isa();
        if (list_categories) print_benchmark_categories();
        if (list_instructions) print_benchmark_instructions();
        return 0;
    }

    if (!params_enough)
    {
        fprintf(stderr, "Error: You must set --thread_pool parameter.\n");
        fprintf(stderr, "You may also set --idle_time parameter.\n");
        fprintf(stderr, "Usage: %s --thread_pool=[xxx] --idle_time=yyy [--include-isa=list] [--exclude-isa=list] [--include-test=list] [--exclude-test=list]\n", argv[0]);
        fprintf(stderr, "       %s --thread_pool=[xxx] --sweep-instruction='Core Computation'\n", argv[0]);
        fprintf(stderr, "       %s --list-categories | --list-instructions\n", argv[0]);
        fprintf(stderr, "       add --save=path [--save-format=txt|csv] to write compact output.\n");
        fprintf(stderr, "[xxx] indicates all cores to benchmark.\n");
        fprintf(stderr, "Example: [0,3,5-8,13-15].\n");
        fprintf(stderr, "idle_time is the interval time(s) between every two benchmarks.\n");
        fprintf(stderr, "idle_time parameter can be ignored, the default value is 0s.\n");
        fprintf(stderr, "test list supports compute,load,cache,freq,multi_issue.\n");
        fprintf(stderr, "isa list supports detected ISA names such as asimd,bf16,sve,SME2.\n");
        fprintf(stderr, "Use --list-categories to list available test and ISA categories.\n");
        fprintf(stderr, "Use --list-instructions to list valid sweep instruction names.\n");
        fprintf(stderr, "save format defaults to csv for .csv paths, otherwise txt.\n");
        fprintf(stderr, "Notice: there must NOT be any spaces.\n");
        exit(0);
    }

    if (save_options.enabled) {
        save_options.path = trim_arg_value(save_options.path);
        if (save_options.path.empty()) {
            fprintf(stderr, "Error: --save path must not be empty.\n");
            return 1;
        }
        if (!save_options.format_set) {
            save_options.format = infer_save_format_from_path(save_options.path);
        }
    }

    cpufb_register_isa();

    if (!sweep_instruction.empty()) {
        return cpubm_do_instruction_sweep(set_of_threads,
            idle_time,
            sweep_instruction,
            save_options) ? 0 : 1;
    }

    return cpubm_do_bench(set_of_threads, idle_time, filter, save_options) ?
        0 : 1;

}
