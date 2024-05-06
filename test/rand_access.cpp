#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h> // For CPU affinity
#include <time.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
//WINDOW 大小 4MB
#define WINDOW_SIZE 128*1024*1024
//buffer 大小 512 Byte
#define BUFFER_SIZE 512
#define BUFFER_NUM BUFFER_SIZE/CACHE_LINE
#define LOOP_TIME 1000000

#define PTR_BITS 3
#define MAX_RAND 100000

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}
int64_t get_random(int64_t lower_bound, int64_t upper_bound) {
    return lower_bound + rand() % (upper_bound - lower_bound + 1);
}

void *random_access(void *arg) {
    srand(time(NULL));
    struct timespec start, end;
    double time_used=1,pre_time_used=1;
    int i,j,k;
    for(int win_size=1024;win_size<=WINDOW_SIZE;win_size*=2){
        int64_t *ptr = (int64_t*)malloc(win_size);
        int total_num=(win_size)>>3;
        // for(int i=0;i<total_num;i++)
        //     ptr[i]=-1;
        memset(ptr, -1, win_size);
        int64_t index=0;
        //init
        for( j = 0 ; j < total_num/2 ; j++){
            int64_t rand;
            int k;
            for(k=0;k<MAX_RAND;k++){
                rand=get_random(0,total_num-1);
                if(ptr[rand]==-1&&rand!=index){
                    break;
                }
            }
            if(k==MAX_RAND){
                rand=index+1;
                while(ptr[rand]!=-1||rand==index){
                    rand=(rand+1)%total_num;
                }
            }
            ptr[index]=rand;
            index=rand;
            
        }
        ptr[index] = 0;
        //warm up
        index = 0;
        for(k=0 ; k<LOOP_TIME ; k++){
            index = ptr[index];
            
        }
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        index = 0;
        for(k=0 ; k<LOOP_TIME ; k++){
            index = ptr[index];
        }
        
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        time_used = get_time(&start, &end);
        printf("%.10f\n",time_used);
        free(ptr);
    }
    return NULL;
}

int main() {

    pthread_t tid;
    if (pthread_create(&tid, NULL, random_access, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(3, &cpuset); // Change 0 to the desired CPU core
    if (pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(tid, NULL) != 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    return 0;
}
