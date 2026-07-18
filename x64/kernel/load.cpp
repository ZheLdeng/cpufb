#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <immintrin.h>
#include <algorithm>
#include <random>

#include "cacheline_probe.hpp"
#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include "load.hpp"

#include <fstream>
#include <sstream>
#ifdef __linux__
#include<sys/syscall.h>
#endif
//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
//WINDOW 大小 4MB
#define WINDOW_SIZE 16 * 1024 * 1024
#define LOOP_TIME 200000
#define PROBE_REPEATS 7

#define STRIDE 8

#define PTR_BITS 3
#define MAX_RAND 100000

#define MULTIWAY_MIN_LINES 16
#define MULTIWAY_MAX_LINES 64
#define MULTIWAY_JUMP_THRESHOLD 0.25

using namespace std;

static inline void shuffleVector(std::vector<int64_t>& vec) {
    // 使用当前时间作为随机数种子
    std::srand(static_cast<unsigned>(std::time(0)));

    // Fisher-Yates 洗牌算法
    for (size_t i = vec.size() - 1; i > 0; --i) {
        int j = std::rand() % (i + 1); // 生成范围 [0, i] 的随机索引
        std::swap(vec[i], vec[j]);    // 交换当前元素与随机索引元素
    }
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

static void flush_cache_line(void* addr) {
    _mm_clflush(addr);  // 使用 CLFLUSH
}

static void finish_cache_line_flush()
{
    _mm_mfence();
}

static inline void init(int64_t *ptr, vector<int64_t> ptr_index, int64_t group)
{
    // cout << "start init" << endl;
    volatile int64_t index = 0;
    volatile int64_t group_size = ptr_index.size() / group;
    if (group > 1) {
        std::vector<int64_t> group_index(group - 1);
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
        std::vector<int64_t> indexs(ptr_index.size());
        for (int64_t m = 0; m < ptr_index.size(); ++m) {
            indexs[m] = m;
        }
        shuffleVector(indexs);
        index = ptr_index[indexs[0]];
        for (int64_t m = 0; m < ptr_index.size() - 1; ++m) {
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
    int total_num = (win_size) >> 6; //每64byte 1个数
    vector<int64_t> ptr_index(total_num) ;

    for (i = 0; i < PROBE_REPEATS; i++) {
        int64_t index = 0;
        // cout << "main loop start " << i << endl;
        for (int64_t m = 0; m < total_num; ++m) {
                ptr_index[m] = m << 3;
        }
        init(ptr, ptr_index, group);
        //warm up
        for (k = 0; k < LOOP_TIME; k++) {
            // std::cout << index << std::endl;
            index = ptr[index];
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        index = 0;
        for (k = 0; k < LOOP_TIME; k++) {
            index = ptr[index];
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        sum_time_used += get_time(&start, &end);
        usleep(1000);
    }
    free(ptr);
    // printf("size = %d, time used = %.10f\n", win_size / 1024, sum_time_used / 100);
    return sum_time_used / PROBE_REPEATS;
}

static void get_slope(vector<double>& time_used, vector<double>& slope)
{
    for (int i = 1; i < time_used.size(); i++) {
        slope.push_back(abs(time_used[i] - time_used[i - 1]) / time_used[i - 1]);
    }
}

static void get_validation(vector<double>& time_used, vector<double>& validation)
{
    for (int i = 1; i < time_used.size(); i++) {
        validation.push_back(abs(time_used[i] - time_used[i - 1]));
    }
}

// 检查点是否是极大值点
static bool isMaximum(const vector<double>& values, int index)
{
    int n = values.size();
    if (index == 0 || index == n - 1) {
        return false; // 如果点是边界点，则不是极大值点
    }
    return values[index] > values[index - 1] && values[index] > values[index + 1];
}

// 找到给定范围内的所有极大值点
static int find_L2_point(const vector<double>& values, int start, int end)
{
    int size = 0;
    for (int i = start + 1; i < end; i++) {
        if (isMaximum(values, i) && values[i] > values [start]) {
            size = i;
            break;
        }
    }
    size = pow(2, size / 2) * (1 + 0.5 * (size % 2));
    return size;
}

static int find_L1_point(const vector<double>& values)
{
    for (int i = 0; i < values.size(); i++) {
        if (values[i] > 0.2) {
            return i;
        }
    }
    return 0;
}

static inline void random_access(vector<double>& time_used) {
    srand(time(NULL));
    for (int win_size = 1024; win_size <= WINDOW_SIZE; win_size *= 2) {
        // int group = max(1, win_size / 1024);
        time_used.push_back(inloop(max(1, win_size / 1024 / 64), win_size));
        time_used.push_back(inloop(max(1, int(win_size * 1.5 / 1024 / 64)), win_size * 1.5));
    }
    return;
}

void get_cacheline(struct CacheData *cache_size, int cpu_id)
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
    read_data(cpu_id, &cache_size->theory_cacheline, "/cache/index0/coherency_line_size");
#endif
    cache_size->test_cacheline = probe_cacheline_size(
        cache_size->theory_cacheline, CACHE_LINE, flush_cache_line,
        finish_cache_line_flush);
}

void get_theory_cache(struct CacheData *cache_size, int cpu_id)
{
#ifdef __linux__
    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_L2, "/cache/index2/size");
    read_data(cpu_id, &cache_size->theory_way,
        "/cache/index0/ways_of_associativity");
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif
    if (cache_size->test_L1 <= 0) cache_size->test_L1 = cache_size->theory_L1;
    if (cache_size->test_L2 <= 0) cache_size->test_L2 = cache_size->theory_L2;
    if (cache_size->test_way <= 0) cache_size->test_way = cache_size->theory_way;
    if (cache_size->test_cacheline <= 0)
        cache_size->test_cacheline = cache_size->theory_cacheline;
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

    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_L2, "/cache/index2/size");
#endif

    random_access(time_used);

    get_slope(time_used, slope);
    get_validation(time_used, validation);
    L1_size_num = find_L1_point(slope);
    // cache_size->test_L1 = pow(2, find_L1_point(slope));
    cache_size->test_L1 = pow(2, L1_size_num / 2) * (1 + 0.5 * (L1_size_num % 2));
    cache_size->test_L2 = find_L2_point(validation, L1_size_num, validation.size());

}

void get_multiway(struct CacheData *cache_size, int cpu_id)
{
    struct timespec start, end;
    double pre_time_used = 0;
    int detected_way = 0;
    int64_t loop_time = LOOP_TIME, test_time = PROBE_REPEATS;

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
    read_data(cpu_id, &cache_size->theory_L1, "/cache/index0/size");
    read_data(cpu_id, &cache_size->theory_cacheline,
        "/cache/index0/coherency_line_size");
#endif

    // One way spans every L1 set exactly once, so this stride maps each
    // address to the same set.  Unlike the old 4 MiB stride, the usual 4 KiB
    // value walks consecutive pages instead of aliasing one L1 DTLB set.
    size_t conflict_stride = 4096;
    if (cache_size->theory_L1 > 0 && cache_size->theory_way > 0) {
        size_t bytes_per_way = static_cast<size_t>(cache_size->theory_L1) *
            1024 / cache_size->theory_way;
        if (bytes_per_way >= static_cast<size_t>(cache_size->theory_cacheline))
            conflict_stride = bytes_per_way;
    }
    int max_lines = MULTIWAY_MIN_LINES;
    if (cache_size->theory_way > 0)
        max_lines = min(MULTIWAY_MAX_LINES,
            max(MULTIWAY_MIN_LINES, cache_size->theory_way + 4));

    for (int line_count = 1; line_count <= max_lines; ++line_count) {
        uint64_t *index = static_cast<uint64_t*>(
            malloc(conflict_stride * line_count));
        if (index == NULL) break;

        vector<int> order(line_count);
        for (int i = 0; i < line_count; ++i) order[i] = i;
        // A shuffled dependency ring prevents sequential-page prefetch and
        // replacement-policy artifacts from creating an early transition.
        mt19937 generator(0x9e3779b9U + line_count);
        shuffle(order.begin(), order.end(), generator);
        const uint64_t stride_words = conflict_stride / sizeof(uint64_t);
        for (int i = 0; i < line_count; ++i) {
            index[order[i] * stride_words] =
                order[(i + 1) % line_count] * stride_words;
        }

        const uint64_t first = order[0] * stride_words;
        uint64_t next = first;
        for (int64_t k = 0; k < loop_time; ++k) {
            next = index[next];
        }

        vector<double> samples;
        samples.reserve(test_time);
        for (int64_t i = 0; i < test_time; ++i) {
            next = first;
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            for (int64_t k = 0; k < loop_time; ++k) {
                next = index[next];
            }
            __asm__ volatile("" : "+r"(next) : : "memory");
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            samples.push_back(get_time(&start, &end));
        }
        sort(samples.begin(), samples.end());
        // Interrupts and scheduler activity only lengthen samples, so the
        // median is a more robust transition signal than their mean.
        const double time_used = samples[samples.size() / 2];
        free(index);

        if (line_count > 1 &&
            time_used / pre_time_used - 1.0 > MULTIWAY_JUMP_THRESHOLD) {
            detected_way = line_count - 1;
            break;
        }
        pre_time_used = time_used;
        detected_way = line_count;
    }
    cache_size->test_way = detected_way;
}

double get_bandwith(uint64_t looptime, double data_size, string type)
{
    struct timespec start, end;
    double time_used, perf;

    data_size /= 2.0;
    if (data_size > 2 * 1024) {
        data_size = 2 * 1024;
    }
    if (data_size <= 0.0) return 0.0;

    uint64_t bytes_per_loop = static_cast<uint64_t>(data_size * 1024);
    const uint64_t target_bytes = 32ULL * 1024 * 1024 * 1024;
    uint64_t effective_looptime = std::max<uint64_t>(1,
        std::min<uint64_t>(looptime, target_bytes / bytes_per_loop));
    float* cache_data = (float*)malloc(bytes_per_loop);

    //Preventing Compiler Optimization
    for (int i = 0; i < data_size * 1024/sizeof(float); i++) {
        cache_data[i] = i;
    }
    int inner_loop = data_size * 1024 / sizeof(float) / (4 * 32);
    void (*kernel)(float*, int, int64_t) = load_vmovups_kernel;
    if (type.find("movss") != string::npos) kernel = load_movss_stream_kernel;
    else if (type.find("xmm") != string::npos) kernel = load_movups_xmm_kernel;
    else if (type.find("zmm") != string::npos) kernel = load_vmovups_zmm_kernel;

    kernel(cache_data, inner_loop, effective_looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    kernel(cache_data, inner_loop, effective_looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    perf = static_cast<double>(effective_looptime) * bytes_per_loop /
        (time_used * freq[0] * 1e9);

    free(cache_data);
    return perf;
}
