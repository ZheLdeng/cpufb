#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iostream>

#include "compute.hpp"
#include "frequency.hpp"
#include "common.hpp"
#include "load.hpp"

#include <fstream>
#include <sstream>

//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
//WINDOW 大小 4MB
#define WINDOW_SIZE 16 * 1024 * 1024
#define LOOP_TIME 100000000

#define STRIDE 8

#define PTR_BITS 3
#define MAX_RAND 100000

#define BUFFER_NUM 16
#define BUFFER_SIZE 4 * 1024 * 1024

using namespace std;

static int64_t get_random(int64_t lower_bound, int64_t upper_bound) {
    return lower_bound + rand() % (upper_bound - lower_bound + 1);
}

static void shuffleVector(std::vector<int64_t>& vec) {
    // 使用当前时间作为随机数种子
    std::srand(static_cast<unsigned>(std::time(0)));

    // Fisher-Yates 洗牌算法
    for (size_t i = vec.size() - 1; i > 0; --i) {
        int j = std::rand() % (i + 1); // 生成范围 [0, i] 的随机索引
        std::swap(vec[i], vec[j]);    // 交换当前元素与随机索引元素
    }
}
static void shuffleGroups(std::vector<int64_t>& vec, int sub) {
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

void init(int64_t *ptr, vector<int64_t> ptr_index, int64_t group)
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
    for (i = 0; i < 100; i++) {
        int64_t index = 0;
        // cout << "main loop start " << i << endl;
        if (group == 1) {
            for (int64_t m = 0; m < total_num; ++m) {
                ptr_index[m] = m << 3;
            }
        } else {
            for (int64_t m = 0; m < total_num; ++m) {
                ptr_index[m] = m << 3;
            }
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
    return sum_time_used / 100;
}



static void random_access(vector<double>& time_used) {
    for (int win_size = 1024; win_size <= WINDOW_SIZE; win_size *= 2) {
        // int group = max(1, win_size / 1024);
        time_used.push_back(inloop(max(1, win_size / 1024 / 64), win_size));
        time_used.push_back(inloop(max(1, int(win_size * 1.5 / 1024 / 64)), win_size * 1.5));
    }
    return;
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


void print_slope(const std::vector<double>& slope) {
    std::cout << "Slope values: " << std::endl;
    for (size_t i = 0; i < slope.size(); ++i) {
        std::cout << slope[i] << "\n";
    }
    std::cout << std::endl; // 换行
}

void get_cachesize(struct CacheData *cache_size, int cpu_id)
{
    vector<double> time_used, validation, slope;
    int L1_size_num = 0;
#ifdef __linux__
    pid_t pid = gettid();
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }

    FILE *fp = NULL;
    char buf[100] = {0};
    string file_path="/sys/devices/system/cpu/cpu"+ std::to_string(cpu_id) +"/cache/index0/size";
    std::ifstream L1_file(file_path);
    if (L1_file) {
        string read_freq = "cat " + file_path;
        fp = popen(read_freq.c_str(), "r");
        if (fp) {
            int ret = fread(buf, 1, sizeof(buf)-1, fp);
            if (ret > 0) {
                // data->theory_freq = std::stod(buf) * 1e-6;
                cache_size->theory_L1 = std::stod(buf);
            }
            pclose(fp);
        }
    }
    file_path="/sys/devices/system/cpu/cpu"+ std::to_string(cpu_id) +"/cache/index2/size";
    std::ifstream L2_file(file_path);
    if (L2_file) {
        string read_freq = "cat " + file_path;
        fp = popen(read_freq.c_str(), "r");
        if (fp) {
            int ret = fread(buf, 1, sizeof(buf)-1, fp);
            if (ret > 0) {
                // data->theory_freq = std::stod(buf) * 1e-6;
                cache_size->theory_L2 = std::stod(buf);
            }
            pclose(fp);
        }
    }
#endif
    //random_access_stride(time_used);
    random_access(time_used);
    // orded_random_access(time_used);
    //random_access_part(time_used);
    // random_access_part_avg(time_used);
    // get_slope(time_used, slope);
    // print_slope(time_used);
    // print_slope(slope);
    // int L1_point = find_L1_point(slope);
    // int L2_point = find_L2_point(slope, L1_point, slope.size());
    // // std::cout << "L1_point: " << L1_point << " L2_point: " << L2_point << std::endl;
    // cache_size->test_L1 = pow(2, L1_point);
    // cache_size->test_L2 = pow(2, L2_point);
    // // cache_size->test_L1 = pow(2, find_L1_point(slope));
    // // cache_size->test_L2 = pow(2, find_L2_point(slope, log2(cache_size->test_L1), slope.size()));
    
    get_slope(time_used, slope);
    get_validation(time_used, validation);
    L1_size_num = find_L1_point(slope);
    // cache_size->test_L1 = pow(2, find_L1_point(slope));
    cache_size->test_L1 = pow(2, L1_size_num / 2) * (1 + 0.5 * (L1_size_num % 2));
    cache_size->test_L2 = find_L2_point(validation, L1_size_num, validation.size());

}

int get_multiway()
{
    struct timespec start, end;
    double time_used = 0, pre_time_used = 0;
    int i, j, k, w;
    int64_t loop_time = 1000000, test_time = 100;
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
        // cout<<time_used<<" "<<time_used/pre_time_used<<endl;
        if (w > 0 && time_used/pre_time_used - 1 > 1e-1) {
            break;
        }
        free(index);
    }
    return w;
}

double get_bandwith(uint64_t looptime, double data_size, string type)
{
    struct timespec start, end;
    double time_used, perf;

    data_size /= 2.0;
    if (data_size > 2 * 1024) {
        data_size = 2 * 1024;
    }
    float* cache_data = (float*)malloc(data_size * 1024);

    //Preventing Compiler Optimization
    for (int i = 0; i < data_size * 1024/sizeof(float); i++) {
        cache_data[i] = i;
    }
    int inner_loop = data_size * 1024 / sizeof(float) / (4 * 32);
#ifdef _SVE_LD1W_
	// warm up
    if (type.find("ld1w")!= string::npos) {
        load_ld1w_kernel(cache_data, data_size * 1024 / sizeof(float), looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_ld1w_kernel(cache_data, data_size * 1024 / sizeof(float), looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    } else {
        load_vmovups_kernel(cache_data, inner_loop, looptime);

        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_vmovups_kernel(cache_data, inner_loop, looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    }
#else
    load_vmovups_kernel(cache_data, inner_loop, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_vmovups_kernel(cache_data, inner_loop, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
#endif
    time_used = get_time(&start, &end);
    perf = (double)looptime * data_size * 1024 / (time_used * freq[0] * 1e9);

    free(cache_data);
    return perf;
}