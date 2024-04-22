#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>
#include <cstdint>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

//cacheline长度
#define CACHE_LINE 64
//测试WINDOW的数量上限
#define WINDOW_NUM 2048
//WINDOW 大小 4MB
#define WINDOW_SIZE 1024
//buffer 大小 512 Byte
#define BUFFER_SIZE 512
#define BUFFER_NUM BUFFER_SIZE/CACHE_LINE
#define LOOP_TIME 1000000

#define PTR_BITS 3

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}

int main(){
    struct timespec start, end;
    double time_used=1,pre_time_used=1;
    int i,j,k;
    int w=30;
    // std::cout<<BUFFER_NUM<<std::endl;
    // std::cout<<WINDOW_SIZE<<std::endl;
    for(int w = 0 ; w < WINDOW_NUM ; w++){
        uint64_t *ptr = (uint64_t*)malloc(WINDOW_SIZE*(w+1));
        uint64_t next=0;
        //init
        for( j = 0 ; j < w ; j++){
            
            ptr[(j * WINDOW_SIZE) >> PTR_BITS ]=((j + 1) * WINDOW_SIZE) >> PTR_BITS;
            // std::cout<<ptr[(j * WINDOW_SIZE) >> PTR_BITS ]<<std::endl;
        }
        ptr[(j * WINDOW_SIZE) >> PTR_BITS] = 0;
        //warm up
        next = 0;
        for(k=0 ; k<LOOP_TIME ; k++){
            next = ptr[next];
        }
        // exit(0);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        next = 0;
        for(k=0 ; k<LOOP_TIME ; k++){
            
            next = ptr[next];
        }
        
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        time_used = get_time(&start, &end);
        // printf("ways = %d, time = %.10f\n", w+1, time_used);
        printf("%.10f\n",time_used);
        free(ptr);
    }

    return 0;
}