
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "../include/thread_pool.h"

/**
 * author: mark 
 * QQ: 2548898954
 * shell: g++ taskqueue_test.cc -o taskqueue_test -lgtest -lgtest_main -lpthread
 */

int done = 0;

pthread_mutex_t lock;

void do_task(void *arg) {
    threadpool_t *pool = (threadpool_t*)arg;
    pthread_mutex_lock(&lock);
    done++;
    printf("doing %d task\n", done);
    pthread_mutex_unlock(&lock);
    if (done >= 1000) {
        thread_pool_terminate(pool);
    }
}

void test_thrdpool_basic() {
    int threads = 8;
    pthread_mutex_init(&lock, NULL);
    threadpool_t *pool = thread_pool_create(threads);
    if (pool == NULL) {
        perror("thread pool create error!\n");
        exit(-1);
    }

    // 限制任务数量，避免内存耗尽
    for (int i = 0; i < 2000; i++) {
        if (thread_pool_post(pool, &do_task, pool) != 0) {
            break;
        }
    }

    thread_pool_waitdone(pool);
    pthread_mutex_destroy(&lock);
}

int main(int argc, char **argv) {
    test_thrdpool_basic();
    return 0;
}
