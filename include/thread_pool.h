#ifndef THREAD_POOL_H
#define THREAD_POOL_H

// 将线程池的实现细节对头文件的用户隐藏起来
// 由于栈结构体不完整，用户无法直接访问其成员，只能通过*来表示
// struct ThreadPool_s的前向声明
typedef struct ThreadPool_s threadpool_t;
// 任务执行的规范 ctx 上下文
typedef void (*handler_pt)(void * /* ctx */);
// 确保在C++编译器下使用C链接方式
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// 对称处理
// 创建线程池
threadpool_t* thread_pool_create(int thread_count);

// 终止线程池
void thread_pool_terminate(threadpool_t* pool);

// 表示将任务投递给线程池
int thread_pool_post(threadpool_t* pool, handler_pt func, void* arg);

// 等待线程池中的所有任务完成
void thread_pool_waitdone(threadpool_t* pool);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // THREAD_POOL_H