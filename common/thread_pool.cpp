#include <stdio.h>
#include <cstdlib>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include "thread_pool.hpp"
#include <algorithm>
#include <iostream>
#ifdef __linux__
#include<sys/syscall.h>
#endif
using namespace std;


static tpool_work_t *tpool_work_create(thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (func == NULL)
        return NULL;

    work = (tpool_work_t*)malloc(sizeof(*work));
    work->func = func;
    work->arg  = arg;
    work->next = NULL;
    return work;
}

static void tpool_work_destroy(tpool_work_t *work)
{
    if (work == NULL)
        return;
    free(work);
}


static tpool_work_t *tpool_work_get(tpool_t *tm)
{
    tpool_work_t *work;

    if (tm == NULL)
        return NULL;

    work = tm->work_first;
    if (work == NULL)
        return NULL;

    if (work->next == NULL) {
        tm->work_first = NULL;
        tm->work_last  = NULL;
    } else {
        tm->work_first = work->next;
    }

    return work;
}

static void *tpool_worker(void *arg)
{
    struct tpool_args* targs = (struct tpool_args*)arg;
    tpool_t      *tm = targs->tm;
    size_t cpu_id=targs->cpuid;
    free(targs);
#ifdef __linux__
    pid_t pid = syscall(SYS_gettid);
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    if (sched_setaffinity(pid, sizeof(cpu_set_t), &mask) < 0) {
        printf("Error: cpu id %d sched_setaffinity\n", cpu_id);
        printf("Warning: performance may be impacted \n");
    }
#endif
    tpool_work_t *work;
    uint64_t observed_parallel_generation = 0;

    pthread_mutex_lock(&(tm->work_mutex));
    tm->startup_ready_cnt++;
    pthread_cond_signal(&(tm->parallel_ready_cond));

    while (1) {
        while (tm->work_first == NULL &&
               tm->parallel_generation == observed_parallel_generation &&
               !tm->stop)
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));

        if (tm->stop)
            break;

        if (tm->parallel_generation != observed_parallel_generation) {
            observed_parallel_generation = tm->parallel_generation;
            thread_func_t parallel_func = tm->parallel_func;
            void *parallel_arg = tm->parallel_arg;

            tm->parallel_ready_cnt++;
            if (tm->parallel_ready_cnt == tm->thread_num)
                pthread_cond_signal(&(tm->parallel_ready_cond));

            while (tm->parallel_start_generation < observed_parallel_generation &&
                   !tm->stop)
                pthread_cond_wait(&(tm->parallel_start_cond), &(tm->work_mutex));

            if (tm->stop)
                break;

            pthread_mutex_unlock(&(tm->work_mutex));
            parallel_func(parallel_arg);
            pthread_mutex_lock(&(tm->work_mutex));

            tm->parallel_done_cnt++;
            if (tm->parallel_done_cnt == tm->thread_num)
                pthread_cond_signal(&(tm->parallel_done_cond));
            continue;
        }

        work = tpool_work_get(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));

        if (work != NULL) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;
        if (!tm->stop && tm->working_cnt == 0 && tm->work_first == NULL)
            pthread_cond_signal(&(tm->working_cond));
    }

    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));
    return NULL;
}

tpool_t *tpool_create(vector<int> set_of_threads)
{
    tpool_t   *tm;
    size_t     i, num;
    num = set_of_threads.size();
    if (num == 0)
        num = 2;

    tm = (tpool_t *)calloc(1, sizeof(*tm));
    tm->thread_cnt = num;
    tm->thread_num = num;
#ifdef __APPLE__
    dispatch_qos_class_t qos_class = QOS_CLASS_USER_INTERACTIVE;
    size_t size = sizeof(int);  
    dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, qos_class, 0);
    tm->queue = dispatch_queue_create("benchmark", attr);

    // 使用任务组来等待所有线程完成
    tm->group = dispatch_group_create();
#endif

    pthread_mutex_init(&(tm->work_mutex), NULL);
    pthread_cond_init(&(tm->work_cond), NULL);
    pthread_cond_init(&(tm->working_cond), NULL);
    pthread_cond_init(&(tm->parallel_ready_cond), NULL);
    pthread_cond_init(&(tm->parallel_start_cond), NULL);
    pthread_cond_init(&(tm->parallel_done_cond), NULL);

    tm->work_first = NULL;
    tm->work_last  = NULL;
    tm->threads = (pthread_t *)calloc(num, sizeof(*tm->threads));

    pthread_mutex_lock(&(tm->work_mutex));
    for (i=0; i<num; i++) {
        tpool_args *args = (tpool_args *)malloc(sizeof(tpool_args));
        args->tm = tm;
        args->cpuid = set_of_threads[i];
        if (pthread_create(&(tm->threads[i]), NULL, tpool_worker, (void *)args) != 0) {
            free(args);
            tm->stop = true;
            pthread_cond_broadcast(&(tm->work_cond));
            pthread_mutex_unlock(&(tm->work_mutex));
            for (size_t j = 0; j < i; j++) pthread_join(tm->threads[j], NULL);
            free(tm->threads);
            pthread_mutex_destroy(&(tm->work_mutex));
            pthread_cond_destroy(&(tm->work_cond));
            pthread_cond_destroy(&(tm->working_cond));
            pthread_cond_destroy(&(tm->parallel_ready_cond));
            pthread_cond_destroy(&(tm->parallel_start_cond));
            pthread_cond_destroy(&(tm->parallel_done_cond));
            free(tm);
            return NULL;
        }
    }

    while (tm->startup_ready_cnt != num)
        pthread_cond_wait(&(tm->parallel_ready_cond), &(tm->work_mutex));
    pthread_mutex_unlock(&(tm->work_mutex));

    return tm;
}

bool tpool_add_work(tpool_t *tm, thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (tm == NULL)
        return false;

    work = tpool_work_create(func, arg);
    if (work == NULL)
        return false;

    pthread_mutex_lock(&(tm->work_mutex));
    if (tm->work_first == NULL) {
        tm->work_first = work;
        tm->work_last  = tm->work_first;
    } else {
        tm->work_last->next = work;
        tm->work_last       = work;
    }
    // tm->thread_cnt += 1;
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return true;
}

void tpool_wait(tpool_t *tm)
{
    if (tm == NULL)
        return;

    pthread_mutex_lock(&(tm->work_mutex));
    while (1) {
        if (tm->work_first != NULL || (!tm->stop && tm->working_cnt != 0) || (tm->stop && tm->thread_cnt != 0)) {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}

bool tpool_run_all(tpool_t *tm, thread_func_t func, void *arg,
    struct timespec *start, struct timespec *end)
{
    if (tm == NULL || func == NULL || start == NULL || end == NULL)
        return false;

    // The parallel generation and the ordinary work queue are deliberately
    // serialized. This keeps existing load/cache users on the queue path while
    // compute benchmarks get fixed one-job-per-worker semantics.
    tpool_wait(tm);
    pthread_mutex_lock(&(tm->work_mutex));

    if (tm->stop) {
        pthread_mutex_unlock(&(tm->work_mutex));
        return false;
    }

    tm->parallel_func = func;
    tm->parallel_arg = arg;
    tm->parallel_ready_cnt = 0;
    tm->parallel_done_cnt = 0;
    tm->parallel_generation++;
    pthread_cond_broadcast(&(tm->work_cond));

    while (tm->parallel_ready_cnt != tm->thread_num && !tm->stop)
        pthread_cond_wait(&(tm->parallel_ready_cond), &(tm->work_mutex));

    if (tm->stop) {
        pthread_mutex_unlock(&(tm->work_mutex));
        return false;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, start);
    tm->parallel_start_generation = tm->parallel_generation;
    pthread_cond_broadcast(&(tm->parallel_start_cond));

    while (tm->parallel_done_cnt != tm->thread_num && !tm->stop)
        pthread_cond_wait(&(tm->parallel_done_cond), &(tm->work_mutex));
    clock_gettime(CLOCK_MONOTONIC_RAW, end);

    bool ok = !tm->stop;
    pthread_mutex_unlock(&(tm->work_mutex));
    return ok;
}

void tpool_destroy(tpool_t *tm)
{
    tpool_work_t *work;
    tpool_work_t *work2;

    if (tm == NULL)
        return;

    pthread_mutex_lock(&(tm->work_mutex));
    work = tm->work_first;
    while (work != NULL) {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }
    tm->work_first = NULL;
    tm->stop = true;
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_cond_broadcast(&(tm->parallel_start_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    for (size_t i = 0; i < tm->thread_num; i++)
        pthread_join(tm->threads[i], NULL);

    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));
    pthread_cond_destroy(&(tm->parallel_ready_cond));
    pthread_cond_destroy(&(tm->parallel_start_cond));
    pthread_cond_destroy(&(tm->parallel_done_cond));

    free(tm->threads);
    free(tm);
}

void parse_thread_pool(char *sets,
    vector<int> &set_of_threads)
{
    if (sets[0] != '[')
    {
        return;
    }
    int pos = 1;
    int left = 0, right = 0;
    int state = 0;
    while (sets[pos] != ']' && sets[pos] != '\0')
    {
        if (state == 0)
        {
            if (sets[pos] >= '0' && sets[pos] <= '9')
            {
                left *= 10;
                left += (int)(sets[pos] - '0');
            }
            else if (sets[pos] == ',')
            {
                set_of_threads.push_back(left);
                left = 0;
            }
            else if (sets[pos] == '-')
            {
                right = 0;
                state = 1;
            }
        }
        else if (state == 1)
        {
            if (sets[pos] >= '0' && sets[pos] <= '9')
            {
                right *= 10;
                right += (int)(sets[pos] - '0');
            }
            else if (sets[pos] == ',')
            {
                int i;
                for (i = left; i <= right; i++)
                {
                    set_of_threads.push_back(i);
                }
                left = 0;
                state = 0;
            }
        }
        pos++;
    }
    if (sets[pos] != ']')
    {
        return;
    }
    if (state == 0)
    {
        set_of_threads.push_back(left);
    }
    else if (state == 1)
    {
        int i;
        for (i = left; i <= right; i++)
        {
            set_of_threads.push_back(i);
        }
    }
}
