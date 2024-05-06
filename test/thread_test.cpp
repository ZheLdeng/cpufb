#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "thread_pool.hpp"
extern "C"
{
void asimd_fmla_vs_f32f32f32(int64_t);
void asimd_fmla_vv_f32f32f32(int64_t);
void asimd_fmla_vs_f64f64f64(int64_t);
void asimd_fmla_vv_f64f64f64(int64_t);
}

static const size_t num_threads = 3;
static const size_t num_items   = 3;
static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(end->tv_nsec - start->tv_nsec) * 1e-9;
}

void wrapper_asimd_fmla_vs_f32f32f32(void* arg) {
    int64_t looptime = *reinterpret_cast<int64_t*>(arg);
    asimd_fmla_vs_f32f32f32(looptime);
}

int main(int argc, char **argv)
{
    struct timespec start, end;
    double time_used, perf;
    tpool_t *tm;
    size_t   i;
    std::vector<int>thread_pool={4,6,7};
    tm   = tpool_create(num_threads,thread_pool);
    long long looptime = 0x100000000LL;
    for (i=0; i<num_items; i++) {
        tpool_add_work(tm, wrapper_asimd_fmla_vs_f32f32f32, &looptime);
    }

    tpool_wait(tm);


    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    for (i=0; i<num_items; i++) {
        // vals[i] = i;
        tpool_add_work(tm, wrapper_asimd_fmla_vs_f32f32f32, &looptime);
    }

    tpool_wait(tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end);
    time_used = get_time(&start, &end);
    perf = looptime * 192 * num_threads /
        time_used * 1e-9;
    printf("time = %lf, perf = %lf\n", time_used, perf);
    // for (i=0; i<num_items; i++) {
    //     printf("%d\n", vals[i]);
    // }

    tpool_destroy(tm);
    return 0;
}