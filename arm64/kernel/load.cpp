#include <algorithm>
#include <climits>
#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <vector>
#include <iostream>
#include<common.hpp>
#include <load.hpp>
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
#define CACHELINE_REPEAT 1000
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
#define CACHELINE_REPEAT 200
#define MULTIWAY_LOOP_TIME 200000
#define MULTIWAY_TEST_TIME 20
#endif

#define PTR_BITS 3
#define MAX_RAND 100000

#define BUFFER_NUM 16
#define BUFFER_SIZE 4 * 1024 * 1024

using namespace std;

double cacheline = 0;
typedef void (*load_bench)(float*, int, int64_t);
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
    if (type.find("ld1w") != string::npos || type.find("ZA") != string::npos) {
        return static_cast<int>(sizeof(float));
    }
    if (type.find("ldp") != string::npos) {
        return 512;
    }
    return 512;
}

#ifdef __APPLE__
static bool read_sysctl_u64(const char *name, uint64_t &value)
{
    uint64_t sysctl_value = 0;
    size_t size = sizeof(sysctl_value);
    if (sysctlbyname(name, &sysctl_value, &size, NULL, 0) == 0 && sysctl_value > 0) {
        value = sysctl_value;
        return true;
    }
    return false;
}

static void read_darwin_cache_info(struct CacheData *cache_data)
{
    uint64_t value = 0;

    if (cache_data->theory_cacheline <= 0) {
        if (read_sysctl_u64("hw.cachelinesize", value) && value <= INT_MAX) {
            cache_data->theory_cacheline = static_cast<int>(value);
        } else {
            cache_data->theory_cacheline = 128;
        }
    }

    if (cache_data->theory_L1 <= 0) {
        if ((read_sysctl_u64("hw.perflevel0.l1dcachesize", value) ||
             read_sysctl_u64("hw.l1dcachesize", value)) &&
            value / 1024 <= INT_MAX) {
            cache_data->theory_L1 = static_cast<int>(value / 1024);
        } else {
            cache_data->theory_L1 = 64;
        }
    }

    if (cache_data->theory_L2 <= 0) {
        if ((read_sysctl_u64("hw.perflevel0.l2cachesize", value) ||
             read_sysctl_u64("hw.l2cachesize", value)) &&
            value / 1024 <= INT_MAX) {
            cache_data->theory_L2 = static_cast<int>(value / 1024);
        } else {
            cache_data->theory_L2 = 4096;
        }
    }
}
#endif

static bool read_text_file(const string &path, string &value)
{
    ifstream input(path);
    if (!input) return false;
    input >> value;
    return !value.empty();
}

static int parse_cache_size_kb(const string &text)
{
    if (text.empty()) return 0;
    char *end = nullptr;
    double value = strtod(text.c_str(), &end);
    if (end == text.c_str() || value <= 0) return 0;
    if (*end == 'M' || *end == 'm') value *= 1024.0;
    else if (*end == 'G' || *end == 'g') value *= 1024.0 * 1024.0;
    if (value > INT_MAX) return 0;
    return static_cast<int>(value);
}

void get_reported_cache_info(struct CacheData *cache_data, int cpu_id)
{
    if (cache_data == nullptr) return;
#ifdef __APPLE__
    (void)cpu_id;
    read_darwin_cache_info(cache_data);
#elif defined(__linux__)
    for (int index = 0; index < 16; ++index) {
        string base = "/sys/devices/system/cpu/cpu" + to_string(cpu_id) +
            "/cache/index" + to_string(index) + "/";
        string level_text, type, size_text, line_text, ways_text;
        if (!read_text_file(base + "level", level_text)) continue;
        read_text_file(base + "type", type);
        read_text_file(base + "size", size_text);
        read_text_file(base + "coherency_line_size", line_text);
        read_text_file(base + "ways_of_associativity", ways_text);

        int level = atoi(level_text.c_str());
        int size_kb = parse_cache_size_kb(size_text);
        int line_size = atoi(line_text.c_str());
        int ways = atoi(ways_text.c_str());
        if (line_size > 0) cache_data->theory_cacheline = line_size;
        if (level == 1 && type == "Data") {
            cache_data->theory_L1 = size_kb;
            cache_data->theory_way = ways;
        } else if (level == 2 && type != "Instruction") {
            cache_data->theory_L2 = max(cache_data->theory_L2, size_kb);
        } else if (level >= 3 && type != "Instruction") {
            cache_data->theory_LLC = max(cache_data->theory_LLC, size_kb);
        }
    }
#else
    (void)cpu_id;
#endif
    if (cache_data->theory_cacheline <= 0) cache_data->theory_cacheline = 64;
    cacheline = cache_data->theory_cacheline;
}

static double median_values(vector<double> values)
{
    if (values.empty()) return 0;
    size_t middle = values.size() / 2;
    nth_element(values.begin(), values.begin() + middle, values.end());
    double result = values[middle];
    if (values.size() % 2 == 0) {
        nth_element(values.begin(), values.begin() + middle - 1, values.end());
        result = (result + values[middle - 1]) / 2.0;
    }
    return result;
}

static vector<uint64_t> build_curve_sizes(const CacheData &cache_data,
    uint64_t max_bytes)
{
    set<uint64_t> sizes;
    for (uint64_t base = 4 * 1024; base <= max_bytes; base *= 2) {
        sizes.insert(base);
        if (base <= max_bytes / 3 * 2) sizes.insert(base + base / 2);
        if (base > max_bytes / 2) break;
    }
    const int reported_kb[] = {
        cache_data.theory_L1, cache_data.theory_L2, cache_data.theory_LLC
    };
    for (int size_kb : reported_kb) {
        if (size_kb <= 0) continue;
        uint64_t bytes = static_cast<uint64_t>(size_kb) * 1024;
        for (int percent : {75, 100, 125}) {
            uint64_t point = bytes * percent / 100;
            point = max<uint64_t>(4 * 1024, point);
            point = (point + 4095) & ~uint64_t(4095);
            if (point <= max_bytes) sizes.insert(point);
        }
    }
    sizes.insert(max_bytes);
    return vector<uint64_t>(sizes.begin(), sizes.end());
}

static double measure_pointer_chase(int64_t *buffer, uint64_t working_set_bytes,
    int line_size, uint64_t seed)
{
    size_t stride = max<size_t>(sizeof(int64_t), static_cast<size_t>(line_size));
    size_t line_count = max<size_t>(2, working_set_bytes / stride);
    size_t stride_words = stride / sizeof(int64_t);
    vector<size_t> order(line_count);
    iota(order.begin(), order.end(), 0);
    mt19937_64 random(seed);
    shuffle(order.begin(), order.end(), random);

    auto word_index = [stride_words](size_t line) { return line * stride_words; };
    for (size_t i = 0; i < line_count; ++i) {
        size_t current = order[i];
        size_t next = order[(i + 1) % line_count];
        buffer[word_index(current)] = static_cast<int64_t>(word_index(next));
    }

    int warmup = static_cast<int>(min<size_t>(max<size_t>(10000, line_count),
        static_cast<size_t>(numeric_limits<int>::max())));
    load_ptr(warmup, buffer);

    int probe_iterations = static_cast<int>(min<size_t>(
        max<size_t>(50000, line_count), 2000000));
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_ptr(probe_iterations, buffer);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    double probe_ns = get_time(&start, &end) * 1e9 / probe_iterations;
    if (!(probe_ns > 0)) return 0;

    size_t target_iterations = static_cast<size_t>(5e6 / probe_ns);
    target_iterations = max<size_t>(50000, target_iterations);
    target_iterations = max(target_iterations, line_count);
    target_iterations = min<size_t>(target_iterations,
        static_cast<size_t>(numeric_limits<int>::max()));
    int iterations = static_cast<int>(target_iterations);
    vector<double> samples;
    samples.reserve(5);
    for (int sample = 0; sample < 5; ++sample) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_ptr(iterations, buffer);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        samples.push_back(get_time(&start, &end) * 1e9 / iterations);
    }
    return median_values(samples);
}

struct CacheJumpCandidate {
    size_t point_index;
    double ratio;
};

static vector<CacheLevelEstimate> estimate_cache_levels(
    const vector<CacheLatencyPoint> &points, const CacheData &cache_data)
{
    vector<CacheJumpCandidate> candidates;
    if (points.size() < 7) return {};
    for (size_t i = 2; i + 3 < points.size(); ++i) {
        vector<double> before, after;
        for (size_t j = i - 2; j <= i; ++j) before.push_back(points[j].latency_ns);
        for (size_t j = i + 1; j <= i + 3; ++j) after.push_back(points[j].latency_ns);
        double before_median = median_values(before);
        double after_median = median_values(after);
        if (before_median <= 0) continue;
        double ratio = after_median / before_median;
        if (ratio >= 1.12 && after_median - before_median >= 0.25)
            candidates.push_back({i, ratio});
    }

    struct SelectedJump {
        CacheJumpCandidate jump;
        string level;
    };
    vector<SelectedJump> selected;
    const pair<const char*, int> reported[] = {
        {"L1", cache_data.theory_L1},
        {"L2", cache_data.theory_L2},
        {"LLC", cache_data.theory_LLC}
    };
    for (const auto &expected : reported) {
        if (expected.second <= 0) continue;
        uint64_t expected_bytes = static_cast<uint64_t>(expected.second) * 1024;
        const CacheJumpCandidate *best = nullptr;
        double best_distance = numeric_limits<double>::max();
        for (const auto &candidate : candidates) {
            size_t capacity_index = candidate.point_index > 0
                ? candidate.point_index - 1 : candidate.point_index;
            uint64_t capacity = points[capacity_index].working_set_bytes;
            if (capacity < expected_bytes / 2 || capacity > expected_bytes * 2)
                continue;
            bool already_used = any_of(selected.begin(), selected.end(),
                [&](const SelectedJump &item) {
                    return item.jump.point_index == candidate.point_index;
                });
            if (already_used) continue;
            double distance = abs(log2(static_cast<double>(capacity) /
                static_cast<double>(expected_bytes)));
            if (distance < best_distance - 1e-9 ||
                (abs(distance - best_distance) < 1e-9 &&
                 (best == nullptr || candidate.ratio > best->ratio))) {
                best = &candidate;
                best_distance = distance;
            }
        }
        if (best != nullptr) selected.push_back({*best, expected.first});
    }

    // Some server cores expose an L2 whose effective single-thread capacity is
    // well below the nominal sysfs size.  Preserve that distinction: if the
    // nominal-size search missed, choose the strongest curve knee above L1 and
    // below the range where an LLC transition could reasonably begin.
    bool has_reported_l2 = cache_data.theory_L2 > 0;
    bool has_selected_l2 = any_of(selected.begin(), selected.end(),
        [](const SelectedJump &item) { return item.level == "L2"; });
    if (has_reported_l2 && !has_selected_l2) {
        uint64_t lower_bound = static_cast<uint64_t>(
            max(cache_data.theory_L1 * 2, 128)) * 1024;
        uint64_t upper_bound = static_cast<uint64_t>(cache_data.theory_L2) *
            1024 * 4;
        if (cache_data.theory_LLC > 0)
            upper_bound = min(upper_bound,
                static_cast<uint64_t>(cache_data.theory_LLC) * 1024 / 4);
        const CacheJumpCandidate *best = nullptr;
        for (const auto &candidate : candidates) {
            size_t capacity_index = candidate.point_index > 0
                ? candidate.point_index - 1 : candidate.point_index;
            uint64_t capacity = points[capacity_index].working_set_bytes;
            if (capacity < lower_bound || capacity > upper_bound) continue;
            bool already_used = any_of(selected.begin(), selected.end(),
                [&](const SelectedJump &item) {
                    return item.jump.point_index == candidate.point_index;
                });
            if (already_used) continue;
            if (best == nullptr || candidate.ratio > best->ratio)
                best = &candidate;
        }
        if (best != nullptr) selected.push_back({*best, "L2"});
    }

    // A shared LLC reported by sysfs can be much larger than the portion that a
    // single pinned thread can effectively use (notably on VMs and partitioned
    // server caches).  If there is no knee near the advertised LLC size, keep
    // the reported value as theory but derive the measured LLC from the
    // strongest remaining post-L2 knee in the actual latency curve.
    bool has_reported_llc = cache_data.theory_LLC > 0;
    bool has_selected_llc = any_of(selected.begin(), selected.end(),
        [](const SelectedJump &item) { return item.level == "LLC"; });
    if (has_reported_llc && !has_selected_llc) {
        uint64_t lower_bound = static_cast<uint64_t>(
            max(cache_data.theory_L2, cache_data.theory_L1)) * 1024 * 2;
        const CacheJumpCandidate *best = nullptr;
        for (const auto &candidate : candidates) {
            size_t capacity_index = candidate.point_index > 0
                ? candidate.point_index - 1 : candidate.point_index;
            uint64_t capacity = points[capacity_index].working_set_bytes;
            if (capacity < lower_bound) continue;
            bool already_used = any_of(selected.begin(), selected.end(),
                [&](const SelectedJump &item) {
                    return item.jump.point_index == candidate.point_index;
                });
            if (already_used) continue;
            if (best == nullptr || candidate.ratio > best->ratio)
                best = &candidate;
        }
        if (best != nullptr) selected.push_back({*best, "LLC"});
    }

    if (selected.empty()) {
        sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
            return a.ratio > b.ratio;
        });
        static const char *fallback_names[] = {"L1", "L2", "LLC"};
        for (const auto &candidate : candidates) {
            size_t capacity_index = candidate.point_index > 0
                ? candidate.point_index - 1 : candidate.point_index;
            uint64_t capacity = points[capacity_index].working_set_bytes;
            bool separated = all_of(selected.begin(), selected.end(),
                [&](const SelectedJump &existing) {
                    size_t other_index = existing.jump.point_index > 0
                        ? existing.jump.point_index - 1 : existing.jump.point_index;
                    uint64_t other = points[other_index].working_set_bytes;
                    return max(capacity, other) >= min(capacity, other) * 4;
                });
            if (separated) {
                selected.push_back({candidate, fallback_names[selected.size()]});
                if (selected.size() == 3) break;
            }
        }
    }
    sort(selected.begin(), selected.end(), [](const auto &a, const auto &b) {
        return a.jump.point_index < b.jump.point_index;
    });

    vector<CacheLevelEstimate> result;
    size_t segment_start = 0;
    for (size_t level = 0; level < selected.size(); ++level) {
        size_t end = selected[level].jump.point_index;
        vector<double> segment;
        for (size_t i = segment_start; i <= end; ++i)
            segment.push_back(points[i].latency_ns);
        size_t capacity_index = end > 0 ? end - 1 : end;
        result.push_back({selected[level].level, points[capacity_index].working_set_bytes,
            median_values(segment), selected[level].jump.ratio});
        segment_start = min(points.size(), end + 1);
    }
    return result;
}

CacheCurveResult measure_cache_hierarchy(struct CacheData *cache_data, int cpu_id)
{
    CacheCurveResult result;
    if (cache_data == nullptr) return result;
    get_reported_cache_info(cache_data, cpu_id);
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
        cerr << "Warning: cache curve could not bind to CPU " << cpu_id << endl;
#endif

    uint64_t reported_max_kb = static_cast<uint64_t>(max({
        cache_data->theory_L1, cache_data->theory_L2, cache_data->theory_LLC, 0
    }));
    uint64_t max_bytes = max<uint64_t>(64ULL * 1024 * 1024,
        reported_max_kb * 1024 * 2);
    max_bytes = min<uint64_t>(max_bytes, 512ULL * 1024 * 1024);
    int line_size = max(64, cache_data->theory_cacheline);
    void *allocation = nullptr;
    if (posix_memalign(&allocation, static_cast<size_t>(line_size), max_bytes) != 0)
        return result;
    int64_t *buffer = static_cast<int64_t*>(allocation);

    vector<uint64_t> sizes = build_curve_sizes(*cache_data, max_bytes);
    for (size_t i = 0; i < sizes.size(); ++i) {
        double latency = measure_pointer_chase(buffer, sizes[i], line_size,
            0x4350554642ULL + i * 0x9e3779b97f4a7c15ULL);
        result.points.push_back({sizes[i], latency});
    }
    free(allocation);
    result.levels = estimate_cache_levels(result.points, *cache_data);
    if (!result.levels.empty()) {
        for (const auto &level : result.levels) {
            int size_kb = static_cast<int>(level.capacity_bytes / 1024);
            if (level.level == "L1") cache_data->test_L1 = size_kb;
            else if (level.level == "L2") cache_data->test_L2 = size_kb;
            else if (level.level == "LLC") cache_data->test_LLC = size_kb;
        }
    }
    vector<double> tail;
    size_t tail_count = min<size_t>(4, result.points.size());
    for (size_t i = result.points.size() - tail_count; i < result.points.size(); ++i)
        tail.push_back(result.points[i].latency_ns);
    result.memory_latency_ns = median_values(tail);
    return result;
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

static inline void flush_cache_line(void *address) {
    asm volatile (
        "dmb ish\n\t"
        "dc civac, %0\n\t"      //clean cacheline
        "dmb ish\n\t"
        "isb\n\t"
        :
        : "r" (address)
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
    struct timespec start, end;
    int i, j, k;
    vector<double> slope;
    int datasize = 64 * 1024;
    vector<double> time_used;
    uintptr_t *ptr = (uintptr_t*)malloc(datasize);
    double first_time, second_time;
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
    read_darwin_cache_info(cache_data);
    cache_data->test_cacheline = cache_data->theory_cacheline;
    cacheline = cache_data->theory_cacheline;
    free(ptr);
    return;
#endif
    for(int buf = 16 ; buf <= 1024 ; buf *= 2){
        first_time = 0;
        second_time = 0;
        int w = datasize / buf;
        int n = (buf >> 3)/2 + 1 ;
        for(j = 0 ; j < datasize >> 3 ; j++){
            ptr[j] = 0;
        }
        uintptr_t* next;
        for( j = 0 ; j < w-1 ; j++){
            ptr[(j * buf) >> 3 ]=(uintptr_t)&ptr[((j + 1) * buf) >> 3];
            ptr[((j * buf) >> 3) + n]=(uintptr_t)&ptr[(((j + 1) * buf) >> 3) + n];
        }
        ptr[(j * buf) >> 3] = (uintptr_t)&ptr[0];
        ptr[((j * buf) >> 3) + n] = (uintptr_t)&ptr[n];
#ifdef __APPLE__
        for(i = 0; i < CACHELINE_REPEAT ; i++){
            for(k = 0; k < datasize >> 3; k++){
                flush_cache_line(&ptr[k]);
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            next = (uintptr_t*)&ptr[0];
            for(k=0 ; k < w ; k++){
                next = (uintptr_t*)*next;
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            first_time +=  (get_time(&start, &end) / w);
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            next = (uintptr_t*)&ptr[n];
            for(k=0 ; k < w ; k++){
                next = (uintptr_t*)*next;
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            second_time += (get_time(&start, &end) / w);
        }
        time_used.push_back(second_time / first_time);
        // cout << "ss: " << buf << " first: " << first_time << " second_time: " << second_time << " ratio: "
            // << second_time / first_time << endl;
    }
    for (size_t i = 0; i < time_used.size() - 1; ++i) {
        if (time_used[i] < 0.98) {
            // cout << i << " " << 16 * pow(2, i) << endl;
            cache_data->test_cacheline = 16 * pow(2, i);
            break;
        }
    }
#else
        for(i = 0; i < CACHELINE_REPEAT ; i++){
            for(k = 0; k < datasize >> 3; k++){
                flush_cache_line(&ptr[k]);
            }
            next = (uintptr_t*)&ptr[0];
            for(k=0 ; k < w ; k++){
                next = (uintptr_t*)*next;
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            next = (uintptr_t*)&ptr[n];
            for(k=0 ; k < w ; k++){
                next = (uintptr_t*)*next;
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            second_time += (get_time(&start, &end) / w);
        }
        time_used.push_back(second_time);
        // cout << "ss: " << buf << " first: " << first_time << " second_time: " << second_time << " ratio: "
            // << second_time / first_time << endl;
    }
    for (size_t i = 1; i < time_used.size() - 1; ++i) {
        if (time_used[i] / time_used[i - 1] > 1.3) {
            // cout << i << " " << 16 * pow(2, i) << endl;
            cache_data->test_cacheline = 16 * pow(2, i - 1);
            break;
        }
    }

#endif
    cacheline = max(cache_data->test_cacheline, cache_data->theory_cacheline);
    free(ptr);
    return;
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
#ifdef __APPLE__ 
    read_darwin_cache_info(cache_size);
    cache_size->test_L1 = cache_size->theory_L1;
    cache_size->test_L2 = cache_size->theory_L2;
    return;
#endif
    random_access(time_used);
    get_slope(time_used, slope);
    get_validation(time_used, validation);
    L1_size_num = find_L1_point(slope);
    // cout << "L1_size = " << L1_size_num << endl;
    cache_size->test_L1 = pow(2, L1_size_num / 2 + 1) * (1 + 0.5 * (L1_size_num % 2));
    // cout << "L1_size == " << cache_size->test_L1 << endl;
    cache_size->test_L2 = find_L2_point(validation, L1_size_num, validation.size());
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
#ifdef __APPLE__
    read_darwin_cache_info(cache_size);
    cache_size->test_way = cache_size->theory_way;
    return;
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

double get_bandwith(uint64_t looptime, double data_size, string type, void* bench)
{
    struct timespec start, end;
    double time_used, perf;
    int inner_loop;
    data_size /= 2.0;
    if (data_size > 32 * 1024) {
        data_size = 32 * 1024;
    }
    size_t data_bytes = static_cast<size_t>(data_size * 1024);
    size_t padding_bytes = 4096;
    float* cache_data = (float*)malloc(data_bytes + padding_bytes);
    if (cache_data == NULL) {
        return 0;
    }

    //Preventing Compiler Optimization
    for (size_t i = 0; i < (data_bytes + padding_bytes) / sizeof(float); i++) {
        cache_data[i] = i;
    }
    inner_loop = static_cast<int>(data_bytes / get_load_bytes_per_inner_loop(type));
    if (inner_loop < 1) {
        inner_loop = 1;
    }
   
    load_bench bench_ptr = reinterpret_cast<load_bench>(bench);
	// warm up
    bench_ptr(cache_data, inner_loop, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    bench_ptr(cache_data, inner_loop, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    perf = !freq.empty() && freq[0] > 0
        ? (double)looptime * data_size * 1024 /
            (time_used * freq[0] * 1e9)
        : -1;
    free(cache_data);
    return perf;
}
