#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h> // 为了 printf (可选)

// 定义函数指针类型
typedef void (*handler_pt)(void *arg);

// 定义自定义自旋锁结构
typedef struct spinlock {
    atomic_int lock;
} spinlock_t;

// 自旋锁加锁实现
static inline void spinlock_lock(spinlock_t *lock) {
    int expected = 0;
    // CAS: 期望是0，如果是0则置为1，否则循环
    while (!atomic_compare_exchange_weak(&lock->lock, &expected, 1)) {
        expected = 0;
        // 可以在这里加 cpu_relax() 或 sched_yield() 优化
    }
}

// 自旋锁解锁实现
static inline void spinlock_unlock(spinlock_t *lock) {
    atomic_store(&lock->lock, 0);
}

// 单向链表任务节点
typedef struct task_s
{
    void *next;         // 下一个任务
    handler_pt func;    // 函数指针，业务逻辑入口
    void *arg;          // 业务逻辑参数 
} task_t;

// 任务队列
typedef struct task_queue_s
{
    void *head;             // 任务链表头
    void **tail;            // 任务链表尾指针
    int block;              // 任务队列阻塞标志
    spinlock_t lock;        // 任务队列自旋锁 (自定义)
    pthread_mutex_t mutex;  // 任务队列条件变量互斥锁
    pthread_cond_t cond;    // 任务队列条件变量
} task_queue_t;

// 线程池管理器
typedef struct thread_pool_s
{
    task_queue_t *task_queue;  // 任务队列
    pthread_t *threads;        // 线程数组
    atomic_int quit;           // 线程池退出标志
    size_t thread_count;       // 线程池线程数量
} threadpool_t; // 加上 typedef 以匹配 worker 中的用法

static task_queue_t *
__task_queue_create(void)
{
    int ret;
    task_queue_t *queue = (task_queue_t *)malloc(sizeof(task_queue_t));
    if (queue)
    {
        ret = pthread_mutex_init(&queue->mutex, NULL);
        if (ret == 0)
        {
            ret = pthread_cond_init(&queue->cond, NULL);
            if (ret == 0)
            {
                // 初始化自旋锁 (使用 C11 原子操作初始化)
                atomic_init(&queue->lock.lock, 0);
                
                queue->head = NULL;
                queue->tail = &queue->head;
                queue->block = 1; // 默认阻塞

                return queue;
            }
            pthread_mutex_destroy(&queue->mutex);
        }
        free(queue);
    }
    return NULL;
}

static void
__nonblock(task_queue_t* queue)
{
    pthread_mutex_lock(&queue->mutex);
    queue->block = 0;
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
}

static void
__add_task(task_queue_t* queue, void* task)
{
    // 这里假设传入的 task 已经是 malloc 好的 task_t*
    void **link = (void **)task;
    *link = NULL;
    
    spinlock_lock(&queue->lock);
    *queue->tail = task;
    queue->tail = link;
    spinlock_unlock(&queue->lock);
    
    pthread_cond_signal(&queue->cond);
}

static inline void *
__pop_task(task_queue_t* queue)
{
    spinlock_lock(&queue->lock);
    if (queue->head == NULL)
    {
        spinlock_unlock(&queue->lock);
        return NULL;
    }

    task_t *task = (task_t *)queue->head;  

    void **link = (void **)task;
    
    queue->head = *link;

    if (queue->head == NULL)
    {
        queue->tail = &queue->head;
    }

    spinlock_unlock(&queue->lock);
    return task;
}

static inline void *
__get_task(task_queue_t* queue)
{
    task_t* task;

    while ((task = (task_t*)__pop_task(queue)) == NULL)
    {
        pthread_mutex_lock(&queue->mutex);
        if (queue->block == 0)
        {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }

        pthread_cond_wait(&queue->cond, &queue->mutex);
        pthread_mutex_unlock(&queue->mutex);
    }
    return task;
}

static void 
__task_queue_destroy(task_queue_t* queue)
{
    task_t* task;
    while ((task = (task_t*)__pop_task(queue)) != NULL)
    {
        free(task);
    }
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

static void *
__thrdpool_worker(void *arg) {
    threadpool_t *pool = (threadpool_t*) arg;
    task_t *task;
    void *ctx;
    while (atomic_load(&pool->quit) == 0) {
        
        task = (task_t*)__get_task(pool->task_queue);
        
        if (!task) break;
        
        handler_pt func = task->func;
        ctx = task->arg;
        
        free(task); 
        
        if (func) {
            func(ctx);
        }
    }
    
    return NULL;
}

static void 
__threads_terminate(threadpool_t* pool)
{
    atomic_store(&pool->quit, 1);
    __nonblock(pool->task_queue);
    for (size_t i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

static int
__threads_create(threadpool_t* pool, size_t thread_count)
{
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret == 0)
    {
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
        if (pool->threads)
        {
            int i = 0;
            for (; i < thread_count; i++)
            {
                if (pthread_create(&pool->threads[i], &attr, __thrdpool_worker, pool) != 0)
                {
                    break;
                }
            }
            pool->thread_count = i;
            pthread_attr_destroy(&attr);
            if (i == thread_count)
            {
                return 0; // 成功创建所有线程
            }
            __threads_terminate(pool);
            free(pool->threads);
            pool->threads = NULL;   
        }
        ret = -1; // 线程创建失败
    }
    return ret;
}

void
thread_pool_terminate(threadpool_t* pool)
{
    atomic_store(&pool->quit, 1);
    __nonblock(pool->task_queue);
}

threadpool_t *
thread_pool_create(int thrd_count) {
    threadpool_t *pool;

    pool = (threadpool_t*)malloc(sizeof(*pool));
    if (pool) {
        task_queue_t *queue = __task_queue_create();
        if (queue) {
            pool->task_queue = queue;
            atomic_init(&pool->quit, 0);
            if (__threads_create(pool, thrd_count) == 0)
                return pool;
            __task_queue_destroy(queue);
        }
        free(pool);
    }
    return NULL;
}

int
thread_pool_post(threadpool_t *pool, handler_pt func, void *arg) {
    if (atomic_load(&pool->quit) == 1) 
        return -1;
    task_t *task = (task_t*) malloc(sizeof(task_t));
    if (!task) return -1;
    task->func = func;
    task->arg = arg;
    __add_task(pool->task_queue, task);
    return 0;
}

void
thread_pool_waitdone(threadpool_t *pool) {
    int i;
    for (i=0; i<pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    __task_queue_destroy(pool->task_queue);
    free(pool->threads);
    free(pool);
}
