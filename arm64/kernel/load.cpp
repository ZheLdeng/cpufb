#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
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
// macOS 上 perf counter 和绑核能力有限，使用较短循环避免整轮 benchmark 过慢
#define WINDOW_SIZE 8 * 1024 * 1024
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

// Safe default; overwritten by get_cacheline() once it runs. Prevents
// log(0) -> -inf in inloop() if both the OS query and the empirical probe
// fail to produce a positive value.
double cacheline = CACHE_LINE;
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
    size_t size = sizeof(int64_t);
    if (sysctlbyname("hw.cachelinesize", &cache_data->theory_cacheline, &size, NULL, 0) != 0) {
        perror("sysctlbyname cachelinesize failed");
    }
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
    // ratio = chain2_time / chain1_time. Chain 1 is cold (just flushed) and
    // brings every line it touches into L1. Chain 2's elements live at
    // offset (buf/2 + 8) within each `buf`-stride window:
    //   * buf <= cacheline:  chain 2 lands inside the same lines chain 1
    //                        already pulled in -> chain 2 = all hits ->
    //                        ratio is small (<< 1).
    //   * buf >  cacheline:  chain 2 lands in lines chain 1 SKIPPED, so
    //                        chain 2 also cold-misses -> ratio ~= 1.
    // The signal is therefore a step UP in ratio at the first buf that
    // exceeds the cache line. Detect that jump (mirrors the Linux branch).
    for (size_t i = 1; i < time_used.size(); ++i) {
        if (time_used[i] / time_used[i - 1] > 2.0) {
            // cout << i << " " << 16 * pow(2, i - 1) << endl;
            cache_data->test_cacheline = 16 * pow(2, i - 1);
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
    for (size_t i = 1; i < time_used.size(); ++i) {
        if (time_used[i] / time_used[i - 1] > 1.3) {
            // cout << i << " " << 16 * pow(2, i) << endl;
            cache_data->test_cacheline = 16 * pow(2, i - 1);
            break;
        }
    }

#endif
    int detected = max(cache_data->test_cacheline, cache_data->theory_cacheline);
    // If both the OS query and the empirical probe failed (returned 0),
    // fall back to a sane default rather than letting log(0) propagate.
    cacheline = detected > 0 ? detected : CACHE_LINE;
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
    size_t size = sizeof(int);
    if (sysctlbyname("hw.perflevel0.l1dcachesize", &cache_size->theory_L1, &size, NULL, 0) != 0) {
        perror("sysctlbyname l1dcachesize failed");
    }
    if (sysctlbyname("hw.perflevel0.l2cachesize", &cache_size->theory_L2, &size, NULL, 0) != 0) {
        perror("sysctlbyname l2cachesize failed");
    }
    cache_size->theory_L1 /= 1024;
    cache_size->theory_L2 /= 1024;

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
    float* cache_data = (float*)malloc(data_size * 1024);

    //Preventing Compiler Optimization
    for (int i = 0; i < data_size * 1024/sizeof(float); i++) {
        cache_data[i] = i;
    }
    inner_loop = static_cast<int>(data_size * 1024 / get_load_bytes_per_inner_loop(type));
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
    perf = (double)looptime * data_size * 1024 / (time_used * freq[0] * 1e9);
    free(cache_data);
    return perf;
}
