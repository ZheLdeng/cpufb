#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <limits>
#include <vector>
#include <atomic>
#include <iostream>
#include <cacheline_probe.hpp>
#include <cache_topology.hpp>
#include<common.hpp>
#include <load.hpp>
#include <thread_pool.hpp>
#include <sstream>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <thread>
#include <sys/sysctl.h>
#endif

#ifdef __linux__
#include<sys/syscall.h>
#endif
//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
#ifndef __APPLE__
//WINDOW 大小 4MB
#define WINDOW_SIZE 4 * 1024 * 1024
#define LOOP_TIME 1000000
#define CACHE_PROBE_REPEAT 100
#define MULTIWAY_LOOP_TIME 1000000
#define MULTIWAY_TEST_TIME 100
#else
// macOS 上 perf counter 和绑核能力有限，使用较短循环避免整轮 benchmark 过慢。
// WINDOW_SIZE 必须大于本机 L2（M-series perf cluster 16-32 MB），否则
// random_access 不会跨过 L2 边界，L2→DRAM 跳变要么不发生、要么落在
// validation 数组末位被 isMaximum 的 boundary 检查吞掉。64 MB 在所有
// M1/M2/M3/M4 上都足以越过 L2 进入 DRAM 区。
#define WINDOW_SIZE 64 * 1024 * 1024
#define LOOP_TIME 200000
#define CACHE_PROBE_REPEAT 20
#define MULTIWAY_LOOP_TIME 200000
#define MULTIWAY_TEST_TIME 20
#endif

#define PTR_BITS 3
#define MAX_RAND 100000

#define BUFFER_NUM 16
#define BUFFER_SIZE 4 * 1024 * 1024

using namespace std;

double cacheline = CACHE_LINE;
typedef void (*load_bench)(float*, int, int64_t);

struct load_bench_result {
    double seconds = 0.0;
    uint64_t cycle_sum = 0;
    size_t cycle_worker_count = 0;
};

struct load_bench_task {
    load_bench bench;
    float* cache_data;
    int inner_loop;
    int64_t looptime;
    size_t worker_stride_bytes;
    std::atomic<size_t> next_worker;
#ifdef __linux__
    std::vector<uint64_t> worker_cycles;
#endif

    load_bench_task(load_bench bench_value, float *cache_data_value,
        int inner_loop_value, int64_t looptime_value,
        size_t worker_stride_bytes_value, size_t worker_count) :
        bench(bench_value),
        cache_data(cache_data_value),
        inner_loop(inner_loop_value),
        looptime(looptime_value),
        worker_stride_bytes(worker_stride_bytes_value),
        next_worker(0)
#ifdef __linux__
        , worker_cycles(worker_count, 0)
#endif
    {
    }
};

static void load_bench_thread_func(void *params)
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    load_bench_task *task = reinterpret_cast<load_bench_task*>(params);
    const size_t worker_index = task->next_worker.fetch_add(1,
        std::memory_order_relaxed);
    float *worker_data = reinterpret_cast<float*>(
        reinterpret_cast<char*>(task->cache_data) +
        worker_index * task->worker_stride_bytes);
#ifdef __linux__
    // Count the load kernel itself instead of inferring its cycle count from
    // a separately calibrated frequency and wall-clock duration.
    PerfEventCycle cycle_counter;
    cycle_counter.start();
#endif
    task->bench(worker_data, task->inner_loop, task->looptime);
#ifdef __linux__
    cycle_counter.stop();
    const long long cycles = cycle_counter.get_cycle();
    if (cycles > 0) {
        task->worker_cycles[worker_index] = static_cast<uint64_t>(cycles);
    }
#endif
}

static load_bench_result run_load_bench(load_bench bench, float* cache_data,
    int inner_loop, int64_t looptime, size_t worker_stride_bytes, tpool_t* tm)
{
    load_bench_result result;
    struct timespec start, end;
    if (tm == nullptr || tm->thread_num == 0) {
#ifdef __linux__
        PerfEventCycle cycle_counter;
#endif
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#ifdef __linux__
        cycle_counter.start();
#endif
        bench(cache_data, inner_loop, looptime);
#ifdef __linux__
        cycle_counter.stop();
        const long long cycles = cycle_counter.get_cycle();
        if (cycles > 0) {
            result.cycle_sum = static_cast<uint64_t>(cycles);
            result.cycle_worker_count = 1;
        }
#endif
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        result.seconds = get_time(&start, &end);
        return result;
    }

    load_bench_task task(bench, cache_data, inner_loop, looptime,
        worker_stride_bytes, tm->thread_num);
    if (!tpool_run_all(tm, load_bench_thread_func, &task, &start, &end)) {
        std::cerr << "Error: failed to run load benchmark on every worker"
                  << std::endl;
        return result;
    }
    result.seconds = get_time(&start, &end);
#ifdef __linux__
    for (const uint64_t cycles : task.worker_cycles) {
        if (cycles > 0) {
            result.cycle_sum += cycles;
            ++result.cycle_worker_count;
        }
    }
#endif
    return result;
}

// Fallback normalization when PMU counters cannot be used.  For a
// multi-worker result, use the participating workers' mean clock rather than
// arbitrarily using the first worker's clock.  On macOS get_cpu_freq() may
// only populate one entry, hence the validity filter.
static double mean_measured_freq_ghz()
{
    double total_ghz = 0.0;
    size_t valid_count = 0;
    for (const double ghz : freq) {
        if (std::isfinite(ghz) && ghz > 0.0) {
            total_ghz += ghz;
            ++valid_count;
        }
    }
    return valid_count == 0 ? 0.0 : total_ghz / valid_count;
}

extern "C" {
    void load_ptr(int looptime, int64_t *ptr);
}

static inline int get_load_bytes_per_inner_loop(const string& type)
{
#ifdef _SVE_
    if (type.find("sve-ld1") != string::npos) {
        return static_cast<int>(16 * load_sve_vector_bytes());
    }
#endif
    if (type.find("neon-ld1") != string::npos) {
        return 256;
    }
    if (type.find("ldrq-4x1") != string::npos ||
        type.find("ldr.q") != string::npos) {
        return 256;
    }
    if (type.find("ld1w") != string::npos || type.find("ZA") != string::npos) {
        return static_cast<int>(sizeof(float));
    }
    if (type.find("ldp") != string::npos) {
        return 512;
    }
    return 512;
}

static inline size_t get_load_workset_alignment(const string& type)
{
#ifdef _SME_
    // ZA slice and SME2 multi-vector kernels use x3 as a scalar-element
    // count, then consume 16 streaming vectors per loop body.  Their local
    // workset must end on that whole-body boundary.
    if (type.find("ZA") != string::npos || type == "ld1w(f32)") {
        return static_cast<size_t>(16 * load_sme_vector_bytes());
    }
#endif
    return static_cast<size_t>(get_load_bytes_per_inner_loop(type));
}

static inline void shuffleVector(std::vector<int64_t>& vec) {
    // 使用当前时间作为随机数种子
    std::srand(static_cast<unsigned>(std::time(0)));

    // Fisher-Yates 洗牌算法
    for (size_t i = vec.size() - 1; i > 0; --i) {
        int j = rand() % (i + 1); // 生成范围 [0, i] 的随机索引
        std::swap(vec[i], vec[j]);    // 交换当前元素与随机索引元素
    }
}

static void flush_cache_line(void *address) {
    asm volatile (
        "dc civac, %0\n\t"      // clean and invalidate cache line
        :
        : "r" (address)
        : "memory"
    );
}

static void finish_cache_line_flush()
{
    asm volatile (
        "dsb ish\n\t"
        "isb\n\t"
        :
        :
        : "memory"
    );
}

static inline void shuffleGroups(std::vector<int64_t>& vec, int sub) {
    if (sub <= 0) {
        std::cerr << "Error: sub must be greater than 0." << std::endl;
        return;
    }
    // 初始化随机数种子
    std::srand(std::time(0));

    // 遍历 vector，将其分为大小为 sub 的组
    for (size_t i = 0; i < vec.size(); i += sub) {
        // 计算当前组的结束位置
        size_t end = std::min(i + sub, vec.size());

        // 对当前组进行洗牌
        for (size_t j = i; j < end; ++j) {
            // 生成范围内的随机索引
            size_t randomIndex = i + (std::rand() % (end - i));
            // 交换当前元素和随机索引处的元素
            std::swap(vec[j], vec[randomIndex]);
        }
    }
}

static inline void init(int64_t *ptr, vector<int64_t> ptr_index, int64_t group)
{
    // cout << "start init" << endl;
    volatile int64_t index = 0;
    volatile int64_t group_size = ptr_index.size() / group;
    if (group > 1) {
        vector<int64_t> group_index(group - 1);
        //group 最后返回0
        for (int64_t m = 0; m < group - 1; ++m) {
            group_index[m] = m + 1;
        }
        // cout << "start shuffle vector" << endl;
        shuffleVector(group_index);
        // cout << "start shuffle groups" << endl;
        shuffleGroups(ptr_index, group_size);
        // cout << "finish shuffle" << endl;
        // 每次,从0开始,按照shuffle的顺序访问子块,最后返回0
        index = ptr_index[0];
        for (int64_t m = 0; m < group_size - 1; ++m) {
            for (int64_t n = 0; n < group - 1; ++n) {
                ptr[index] = ptr_index[group_index[n] * group_size + m];
                index = ptr[index];
            }
            shuffleVector(group_index);
            ptr[index] = ptr_index[m + 1];
            index = ptr[index];
        }
        for (int64_t n = 0; n < group - 1; ++n) {
            ptr[index] = ptr_index[group_index[n] * group_size + group_size - 1];
            index = ptr[index];
        }
        ptr[index] = ptr_index[0];
    } else {
        vector<int64_t> indexs(ptr_index.size());
        for (int64_t m = 0; m < ptr_index.size(); m++) {
            indexs[m] = m;
        }
        shuffleVector(indexs);
        index = ptr_index[indexs[0]];
        for (int64_t m = 0; m < ptr_index.size() - 1; m++) {
            ptr[index] = ptr_index[indexs[m + 1]];
            index = ptr[index];
        }
        ptr[index] = ptr_index[indexs[0]];
    }
    // cout << "finish init" << endl;
}

static inline double inloop(int group, int win_size)
{
    int i, j, k;
    struct timespec start, end;
    double sum_time_used = 0;
    int64_t *ptr = (int64_t*)malloc(win_size);
    int read_stride = int(log(cacheline) / log(2));
    int total_num = (win_size) >> read_stride; //每cacheline byte 1个数

    vector<int64_t> ptr_index(total_num) ;
    for (i = 0; i < CACHE_PROBE_REPEAT; i++) {
        // cout << "main loop start " << i << endl;
        int64_t index = 0;
        for (int64_t m = 0; m < total_num; ++m) {
            ptr_index[m] = m << (read_stride - 3);
        }
        init(ptr, ptr_index, group);
        //warm up
        load_ptr(LOOP_TIME, ptr);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_ptr(LOOP_TIME, ptr);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        sum_time_used += get_time(&start, &end);
        usleep(1000);
    }
    free(ptr);
    // printf("size = %d, time used = %.10f\n", win_size / 1024, sum_time_used / CACHE_PROBE_REPEAT);

    return sum_time_used / CACHE_PROBE_REPEAT;
}

static inline void get_slope(vector<double>& data, vector<double>& slope)
{
    for (int i = 1; i < data.size(); i++) {
        slope.push_back((data[i] - data[i - 1]) / data[i - 1]);
        // cout << "slope is" << i << " is " << abs(data[i] - data[i - 1]) << endl;
    }
}

static inline void get_validation(vector<double>& data, vector<double>& validation)
{
    for (int i = 1; i < data.size(); i++) {
        validation.push_back(abs(data[i] - data[i - 1]));
        // cout << scientific << "validation " << i << " is " << abs(data[i] - data[i - 1]) << endl;
    }
}

// 检查点是否是极大值点
static inline bool isMaximum(const vector<double>& values, int index)
{
    int n = values.size();
    if (index == 0 || index == n - 1) {
        return false; // 如果点是边界点，则不是极大值点
    }
    return values[index] > values[index - 1] && values[index] > values[index + 1];
}

// 找到给定范围内的所有极大值点
static inline int find_L2_point(const vector<double>& values, int start, int end)
{
    int size = 0;
    // cout << "start end " << start << " " << end << " " << values[start] << endl;
    for (int i = start + 1; i < end; i++) {

        if (isMaximum(values, i) && values[i] > values [start]) {
            size = i;
            break;
        }
    }
    // cout << "size = " << size << endl;
    size = pow(2, size / 2 + 1) * (1 + 0.5 * (size % 2));
    return size;
}

static inline int find_L1_point(const vector<double>& values)
{
    for (int i = 0; i < values.size(); i++) {
        if (values[i] > 0.2) {
            return i;
        }
    }
    return 0;
}

static inline void random_access(vector<double>& time_used) {
    for (int win_size = 2 * 1024; win_size <= WINDOW_SIZE; win_size *= 2) {
        // cout << "win_size = " << win_size << " " << int(win_size * 1.5 / 1024 / 64) << endl;
        time_used.push_back(inloop(max(1, win_size / 1024 / 64), win_size));
        time_used.push_back(inloop(max(1, int(win_size * 1.5 / 1024 / 64)), win_size * 1.5));
    }
    return;
}

void get_cacheline(struct CacheData *cache_data, int cpu_id)
{
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }
    read_data(cpu_id, &cache_data->theory_cacheline, "/cache/index0/coherency_line_size");
#endif
#ifdef __APPLE__ 
    uint64_t cacheline_bytes = 0;
    size_t size = sizeof(cacheline_bytes);
    if (sysctlbyname("hw.cachelinesize", &cacheline_bytes, &size, NULL, 0) != 0) {
        perror("sysctlbyname cachelinesize failed");
    } else if (cacheline_bytes <=
            static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        cache_data->theory_cacheline = static_cast<int>(cacheline_bytes);
    }
#endif
    const int fallback_cacheline =
#ifdef __APPLE__
        128;
#else
        CACHE_LINE;
#endif
    cache_data->test_cacheline = probe_cacheline_size(
        cache_data->theory_cacheline, fallback_cacheline, flush_cache_line,
        finish_cache_line_flush);
    cacheline = cache_data->test_cacheline > 0
        ? cache_data->test_cacheline
        : CACHE_LINE;
}

void get_cachesize(struct CacheData *cache_size, int cpu_id)
{
    vector<double> time_used, validation, slope;
    int L1_size_num = 0;
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }
#endif
    get_cache_capacities(cache_size, cpu_id);
    random_access(time_used);
    get_slope(time_used, slope);
    get_validation(time_used, validation);
    L1_size_num = find_L1_point(slope);
    // cout << "L1_size = " << L1_size_num << endl;
    cache_size->test_L1 = pow(2, L1_size_num / 2 + 1) * (1 + 0.5 * (L1_size_num % 2));
    // cout << "L1_size == " << cache_size->test_L1 << endl;
    cache_size->test_L2 = find_L2_point(validation, L1_size_num, validation.size());
}

void get_cache_capacities(struct CacheData *cache_size, int cpu_id)
{
    if (cache_size == nullptr) return;

    const cpufb::CacheLevelInfo l1 =
        cpufb::detect_data_cache_level(cpu_id, 1);
    const cpufb::CacheLevelInfo l2 =
        cpufb::detect_data_cache_level(cpu_id, 2);
    const std::uint64_t max_kib =
        static_cast<std::uint64_t>(std::numeric_limits<int>::max());

    if (l1.bytes >= 1024 && l1.bytes / 1024 <= max_kib) {
        cache_size->theory_L1 = static_cast<int>(l1.bytes / 1024);
        cache_size->theory_L1_source = l1.source;
    }
    if (l2.bytes >= 1024 && l2.bytes / 1024 <= max_kib) {
        cache_size->theory_L2 = static_cast<int>(l2.bytes / 1024);
        cache_size->theory_L2_source = l2.source;
    }
}

void get_multiway(struct CacheData *cache_size, int cpu_id)
{
    struct timespec start, end;
    double time_used = 0, pre_time_used = 0;
    int i, j, k, w;
    int64_t loop_time = MULTIWAY_LOOP_TIME, test_time = MULTIWAY_TEST_TIME;
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }
    read_data(cpu_id, &cache_size->theory_way, "/cache/index0/ways_of_associativity");
#endif
    for (w = 0; w < BUFFER_NUM; w++) {
        uint64_t *index = (uint64_t*)malloc(BUFFER_SIZE * (w + 1));
        uint64_t next = 0;
        //init
        for ( j = 0; j < w; j++) {
            index[(j * BUFFER_SIZE) >> 3 ] = ((j + 1) * BUFFER_SIZE) >> 3;
        }
        index[(j * BUFFER_SIZE) >> 3] = 0;
        //warm up
        next = 0;
        for (k = 0; k < loop_time; k++) {
            next = index[next];
        }

        pre_time_used = time_used;
        time_used = 0;
        for (i = 0; i < test_time;i++) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            next = 0;
            for (k = 0; k<loop_time; k++) {
                next = index[next];
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            time_used += get_time(&start, &end);
        }
        time_used /= test_time;
        // cout << "multi way " << w << " / " << time_used << " " << pre_time_used << " / "<< 
            // time_used/pre_time_used << endl;
        if (w > 1 && time_used/pre_time_used - 1 > 1e-1) {
            break;
        }
        free(index);
    }
    cache_size->test_way = w;
    return;
}

double get_bandwith(uint64_t looptime, double data_size, string type, void* bench, tpool_t* tm)
{
    double perf;
    double best_time_used = 0.0;
    uint64_t best_cycle_sum = 0;
    size_t best_cycle_worker_count = 0;
    int inner_loop;
    // Keep every instruction sample long enough for a stable clock reading, but
    // do not let a large LLC/L2 workset turn the full instruction table into a
    // multi-minute benchmark.  One outer loop walks the whole workset once.
    constexpr uint64_t kTargetBytesPerSample = 1ULL << 30; // 1 GiB
    data_size /= 2.0;
    if (data_size > 32 * 1024) {
        data_size = 32 * 1024;
    }
    const size_t requested_data_bytes =
        static_cast<size_t>(data_size) * 1024U;
    // Several hand-written, unrolled SME/SVE load kernels issue their last
    // vector load just past the logical end of the walk.  Keep a guard region
    // so a workset ending at a page boundary cannot fault; only data_bytes is
    // counted in the reported bandwidth.
    constexpr size_t kLoadGuardBytes = 4096;
    const size_t thread_num = (tm != nullptr && tm->thread_num > 0) ?
        tm->thread_num : 1;
    const size_t workset_alignment = get_load_workset_alignment(type);
    // Cache levels in the load table describe the working set seen by one
    // core.  Keep that per-worker working set constant during a scaling run;
    // otherwise a high-thread-count L2 sample can shrink below L1 capacity.
    // Workers still use disjoint storage, so aggregate traffic scales with
    // the worker count without sharing cache lines.
    const size_t data_bytes = requested_data_bytes / workset_alignment *
        workset_alignment;
    if (data_bytes == 0) return 0.0;
    const size_t aggregate_data_bytes = data_bytes * thread_num;
    const uint64_t bytes_per_outer_loop =
        static_cast<uint64_t>(data_bytes);
    uint64_t measured_looptime = max<uint64_t>(1,
        kTargetBytesPerSample / bytes_per_outer_loop);
    measured_looptime = min<uint64_t>(measured_looptime, looptime);
    const size_t worker_stride_bytes = data_bytes + kLoadGuardBytes;
    if (worker_stride_bytes > std::numeric_limits<size_t>::max() / thread_num)
        return 0.0;
    float* cache_data = (float*)malloc(worker_stride_bytes * thread_num);
    if (cache_data == nullptr) return 0.0;

    // Each worker owns a disjoint, equal-sized workset.  This avoids
    // synchronized reads of the same cache lines and keeps every worker's
    // stream in the selected per-core cache level.
    for (size_t worker = 0; worker < thread_num; ++worker) {
        float *worker_data = reinterpret_cast<float*>(
            reinterpret_cast<char*>(cache_data) + worker * worker_stride_bytes);
        for (size_t i = 0; i < data_bytes / sizeof(float); i++) {
            worker_data[i] = static_cast<float>(i + worker);
        }
    }
    inner_loop = static_cast<int>(data_bytes /
        get_load_bytes_per_inner_loop(type));
    if (inner_loop < 1) {
        inner_loop = 1;
    }
   
    load_bench bench_ptr = reinterpret_cast<load_bench>(bench);
	// warm up
    run_load_bench(bench_ptr, cache_data, inner_loop, measured_looptime,
        worker_stride_bytes, tm);
#ifdef __APPLE__
    constexpr int repeat = 5;
#else
    constexpr int repeat = 10;
#endif
    for (int i = 0; i < repeat; i++) {
        const load_bench_result sample = run_load_bench(bench_ptr, cache_data, inner_loop,
            measured_looptime, worker_stride_bytes, tm);
        if (sample.seconds > 0.0 &&
            (best_time_used == 0.0 || sample.seconds < best_time_used)) {
            best_time_used = sample.seconds;
        }
        if (sample.cycle_worker_count == thread_num && sample.cycle_sum > 0 &&
            (best_cycle_sum == 0 || sample.cycle_sum < best_cycle_sum)) {
            best_cycle_sum = sample.cycle_sum;
            best_cycle_worker_count = sample.cycle_worker_count;
        }
    }
    if (best_cycle_worker_count == thread_num) {
        // cycle_sum/thread_num is the mean cycle count over workers.  Dividing
        // aggregate data by that mean reports aggregate Byte/Cycle while using
        // the PMU counts taken around the actual load kernels.
        perf = (double)measured_looptime * aggregate_data_bytes * thread_num /
            best_cycle_sum;
    } else {
        const double mean_freq_ghz = mean_measured_freq_ghz();
        perf = best_time_used > 0.0 && mean_freq_ghz > 0.0
            ? (double)measured_looptime * aggregate_data_bytes /
                (best_time_used * mean_freq_ghz * 1e9)
            : 0.0;
    }
    free(cache_data);
    return perf;
}
