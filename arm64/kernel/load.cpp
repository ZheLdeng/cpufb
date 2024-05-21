#include <stdio.h>
#include <cstdlib>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <time.h>
#include <cstdint>
#include <cmath>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <sched.h>
//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
//WINDOW 大小 4MB
#define WINDOW_SIZE 4*1024*1024
//buffer 大小 512 Byte
#define BUFFER_SIZE 512
#define BUFFER_NUM BUFFER_SIZE/CACHE_LINE
#define LOOP_TIME 10000000

#define PTR_BITS 3
#define MAX_RAND 100000

using namespace std;

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}

static int64_t get_random(int64_t lower_bound, int64_t upper_bound) {
    return lower_bound + rand() % (upper_bound - lower_bound + 1);
}

static void random_access(vector<double>& time_used) {
    srand(time(NULL));
    struct timespec start, end;

    double pre_time_used = 0;
    int i, j, k;
    for(int win_size = 1024; win_size <= WINDOW_SIZE; win_size *= 2) {
        int64_t *ptr = (int64_t*)malloc(win_size);
        int total_num = (win_size) >> 3;
        memset(ptr, -1, win_size);
        int64_t index = 0;
        //init
        for( j = 0 ; j < total_num/2 ; j++){
            int64_t rand;
            int k;
            for(k = 0; k < MAX_RAND; k++){
                rand = get_random(0, total_num-1);
                if(ptr[rand] == -1 && rand != index){
                    break;
                }
            }
            if(k == MAX_RAND){
                rand = index + 1;
                while(ptr[rand] != -1||rand == index){
                    rand = (rand + 1) % total_num;
                }
            }
            ptr[index] = rand;
            index = rand;
            
        }
        ptr[index] = 0;
        //warm up
        index = 0;
        for(k=0 ; k < LOOP_TIME; k++){
            index = ptr[index];
            
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        index = 0;
        for(k = 0 ; k < LOOP_TIME; k++){
            index = ptr[index];
        }
        
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        time_used.push_back(get_time(&start, &end));
        
        // printf("size = %d , time used = %.10f, slope = %.10f\n", win_size / 1024, get_time(&start, &end) / 100, (time_used[time_used.size() - 1] - pre_time_used) / pre_time_used);
        // pre_time_used = get_time(&start, &end);
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
static bool isMaximum(const std::vector<double>& values, int index)
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


void get_cachesize(int *cache_size, int cpu_id)
{
    vector<double> time_used, slope;
    int size_num = log2(WINDOW_SIZE / 1024) + 1;
    pid_t pid = gettid();
    cpu_set_t mask;  
    CPU_ZERO(&mask);  
    CPU_SET(cpu_id, &mask);  
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {  
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);  
        printf("Warning: performance may be impacted \n");  
    } 
    random_access(time_used);
    get_slope(time_used, slope);
    cache_size[0] = pow(2, find_L1_point(slope));
    cache_size[1] = pow(2, find_L2_point(slope, log2(cache_size[0]), slope.size()));

    // cout << "L1 = " << cache_size[0] << " L2 = " << cache_size[1] << endl;
}