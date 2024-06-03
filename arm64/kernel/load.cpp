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
#include <fstream>
#include <sstream>

//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
//WINDOW 大小 4MB
#define WINDOW_SIZE 4 * 1024 * 1024
#define LOOP_TIME 1000000

#define PTR_BITS 3
#define MAX_RAND 100000

#define BUFFER_NUM 16
#define BUFFER_SIZE 4 * 1024 * 1024

using namespace std;

static int64_t get_random(int64_t lower_bound, int64_t upper_bound) {
    return lower_bound + rand() % (upper_bound - lower_bound + 1);
}

static void random_access(vector<double>& time_used) {
    srand(time(NULL));
    struct timespec start, end;
    double sum_time_used = 0;
    int i, j, k;
    for (int win_size = 1024; win_size <= WINDOW_SIZE; win_size *= 2) {
        int64_t *ptr = (int64_t*)malloc(win_size);
        int total_num = (win_size) >> 3;
        memset(ptr, -1, win_size);
        volatile int64_t index = 0;
        //init
        for (j = 0; j < total_num / 2; j++) {
            volatile int64_t rand;
            int k;
            for (k = 0; k < MAX_RAND; k++) {
                rand = get_random(0, total_num - 1);
                if (ptr[rand] == -1 && rand != index) {
                    break;
                }
            }
            if (k == MAX_RAND) {
                rand = index + 1;
                while(ptr[rand] != -1 || rand == index) {
                    rand = (rand + 1) % total_num;
                }
            }
            ptr[index] = rand;
            index = rand;
            
        }
        ptr[index] = 0;
        //warm up
        index = 0;
        for (k = 0; k < LOOP_TIME; k++) {
            index = ptr[index];
            
        }
        sum_time_used = 0;
        for (i = 0; i < 100; i++) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            index = 0;
            for (k = 0; k < LOOP_TIME; k++) {
                index = ptr[index];
            }
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            sum_time_used += get_time(&start, &end);
        }
        time_used.push_back(sum_time_used / 100);
        
        free(ptr);
    }
    return;
}

static void get_slope(vector<double>& time_used, vector<double>& slope)
{
    for (int i = 1; i < time_used.size(); i++) {
        slope.push_back(abs(time_used[i] - time_used[i - 1]) / time_used[i - 1]); 
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
    for (int i = start + 1; i < end; i++) {
        if (isMaximum(values, i)) {
            return i;
        }
    }
    return 0;
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


void get_cachesize(struct CacheData *cache_size, int cpu_id)
{
    vector<double> time_used, slope;
    int size_num = log2(WINDOW_SIZE / 1024) + 1;
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
    random_access(time_used);
    get_slope(time_used, slope);
    cache_size->test_L1 = pow(2, find_L1_point(slope));
    cache_size->test_L2 = pow(2, find_L2_point(slope, log2(cache_size->test_L1), slope.size()));

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
    if (type.find("SVE")!= string::npos) {
        load_ld1w_kernel(cache_data, data_size / sizeof(float), looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_ld1w_kernel(cache_data, data_size / sizeof(float), looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    } else {
        load_ldp_kernel(cache_data, inner_loop, looptime);

        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        load_ldp_kernel(cache_data, inner_loop, looptime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    }
#else
    load_ldp_kernel(cache_data, inner_loop, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    load_ldp_kernel(cache_data, inner_loop, looptime);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
#endif
    time_used = get_time(&start, &end);
    perf = (double)looptime * data_size * 1024 / (time_used * freq[0] * 1e9);

    free(cache_data);
    return perf;
}