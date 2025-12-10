#ifndef SKYNET_SPINLOCK_Y
#define SKYNET_SPINLOCK_Y

#define SPIN_INIT(q) spinlock_init(&(q)->lock)
#define SPIN_LOCK(q) spinlock_lock(&(q)->lock);
#define SPIN_UNLOCK(q) spinlock_unlock(&(q)->lock);
#define SPIN_DESTROY(q) spinlock_destroy(&(q)->lock);

// 允许开发者或构建系统选择使用或禁用 Pthread 库的互斥锁实现
// 如果定义了 USE_PTHREAD_LOCK，则使用 Pthread 互斥锁
#ifndef USE_PTHREAD_LOCK

#ifdef  __STDC_NO_ATOMIC__

#define atomic_flag_ int
#define ATOMIC_FLAG_INIT_ 0
#define atomic_flag_test_and_set_(ptr) __sync_lock_test_and_set(ptr, 1)
#define atomic_flag_clear_(ptr) __sync_lock_release(ptr)

struct spinlock
{
    atomic_flag_ lock;
};

static inline void
spinlock_init(struct spinlock *lock) {
	atomic_flag_ v = ATOMIC_FLAG_INIT_;
	lock->lock = v;
}

static inline void
spinlock_lock(struct spinlock *lock) {
	while (atomic_flag_test_and_set_(&lock->lock)) {}
}

static inline int
spinlock_trylock(struct spinlock *lock) {
	return atomic_flag_test_and_set_(&lock->lock) == 0;
}

static inline void
spinlock_unlock(struct spinlock *lock) {
	atomic_flag_clear_(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
	(void) lock;
}

#else // __STDC_NO_ATOMIC__

#include "atomic.h"

#define atomic_test_and_set_(ptr) STD_ atomic_exchange_explicit(ptr, 1, STD_ memory_order_acquire)
#define atomic_clear_(ptr) STD_ atomic_store_explicit(ptr, 0, STD_ memory_order_release);
// 原子地读取一个共享变量的值
#define atomic_load_relaxed_(ptr) STD_ atomic_load_explicit(ptr, STD_ memory_order_relaxed)

#if defined(__x86_64__)
#include <immintrin.h> // For _mm_pause
#define atomic_pause_() _mm_pause()
#else
#define atomic_pause_() ((void)0)
#endif

struct spinlock {
	STD_ atomic_int lock;
};
static inline void
spinlock_init(struct spinlock *lock)
{
    STD_ atomic_init(&lock->lock, 0);
}

static inline void
spinlock_lock(struct spinlock *lock) {
	for (;;) {
		if (!atomic_test_and_set_(&lock->lock))
			return;
		while (atomic_load_relaxed_(&lock->lock))
			atomic_pause_();
	}
}

static inline int
spinlock_trylock(struct spinlock *lock)
{
    return !atomic_load_relaxed_(&lock->lock) && !atomic_test_and_set_(&lock->lock);
}

static inline void
spinlock_unlock(struct spinlock* lock)
{
    atomic_clear_(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock* lock)
{
    (void)lock;
}

#endif // __STDC_NO_ATOMIC__

#else // USE_PTHREAD_LOCK

#include <pthread.h>

struct spinlock {
	pthread_mutex_t lock;
};

static inline void
spinlock_init(struct spinlock *lock) {
	pthread_mutex_init(&lock->lock, NULL);
}

static inline void
spinlock_lock(struct spinlock *lock) {
	pthread_mutex_lock(&lock->lock);
}

static inline int
spinlock_trylock(struct spinlock *lock) {
	return pthread_mutex_trylock(&lock->lock) == 0;
}

static inline void
spinlock_unlock(struct spinlock *lock) {
	pthread_mutex_unlock(&lock->lock);
}

static inline void
spinlock_destroy(struct spinlock *lock) {
	pthread_mutex_destroy(&lock->lock);
}

#endif // USE_PTHREAD_LOCK

#endif // SKYNET_SPINLOCK_Y