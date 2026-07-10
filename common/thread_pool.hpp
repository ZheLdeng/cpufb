#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <vector>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <thread>
#include <sys/sysctl.h>
#endif
typedef void (*thread_func_t)(void *arg);

struct tpool_work {
    thread_func_t      func;
    void              *arg;
    struct tpool_work *next;
};
typedef struct tpool_work tpool_work_t;

struct tpool {
    tpool_work_t    *work_first;
    tpool_work_t    *work_last;
    pthread_mutex_t  work_mutex;
    pthread_cond_t   work_cond;
    pthread_cond_t   working_cond;
    pthread_cond_t   parallel_ready_cond;
    pthread_cond_t   parallel_start_cond;
    pthread_cond_t   parallel_done_cond;
    size_t           working_cnt;
    size_t           thread_cnt;
    size_t           thread_num;
    size_t           startup_ready_cnt;
    size_t           parallel_ready_cnt;
    size_t           parallel_done_cnt;
    uint64_t         parallel_generation;
    uint64_t         parallel_start_generation;
    thread_func_t    parallel_func;
    void            *parallel_arg;
    pthread_t       *threads;
#ifdef __APPLE__
    dispatch_group_t group;
    dispatch_queue_t queue;
#endif
    bool             stop;
};
typedef struct tpool tpool_t;

struct tpool_args {
    tpool_t * tm;
    size_t cpuid;
};

void parse_thread_pool(char *sets, std::vector<int> &set_of_threads);

tpool_t *tpool_create(std::vector<int>thread_pool);
void tpool_destroy(tpool_t *tm);

bool tpool_add_work(tpool_t *tm, thread_func_t func, void *arg);
void tpool_wait(tpool_t *tm);

// Run the same job exactly once on every pinned worker. The timestamps cover
// only the synchronized execution window: all workers are ready before start
// is recorded, and end is recorded after the last worker completes.
bool tpool_run_all(tpool_t *tm, thread_func_t func, void *arg,
    struct timespec *start, struct timespec *end);

#endif /* __TPOOL_H__ */
