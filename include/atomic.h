#ifndef SKYNET_ATOMIC_H
#define SKYNET_ATOMIC_H

//检查编译器是否支持 C/C++ 标准库中的原子操作（Atomics功能)
#ifdef __STDC_NO_ATOMICS__

#include <stddef.h>
#include <stdint.h>

#define ATOM_INT volatile int
#define ATOM_POINTER volatile uintptr_t
#define ATOM_SIZET volatile size_t
#define ATOM_ULONG volatile unsigned long
#define ATOM_INIT(ptr, v) (*(ptr) = (v))
#define ATOM_LOAD(ptr) (*(ptr))
#define ATOM_STORE(ptr, v) (*(ptr) = (v))
// 原子地比较并交换
#define ATOM_CAS(ptr, oval, nval) __sync_bool_compare_and_swap((ptr), (oval), (nval))
#define ATUO_CAS_ULONG(ptr, oval, nval) __sync_bool_compare_and_swap((ptr), (oval), (nval))
#define ATOM_CAS_SIZET(ptr, oval, nval) __sync_bool_compare_and_swap((ptr), (oval), (nval))
#define ATOM_CAS_POINTER(ptr, oval, nval) __sync_bool_compare_and_swap((ptr), (oval), (nval))
// 原子地增加
#define ATOM_FINC(ptr) __sync_fetch_and_add((ptr), 1)
#define ATOM_FDEC(ptr) __sync_fetch_and_sub((ptr), 1)
#define ATOM_FADD(ptr,n) __sync_fetch_and_add(ptr, n)
#define ATOM_FSUB(ptr,n) __sync_fetch_and_sub(ptr, n)
#define ATOM_FAND(ptr,n) __sync_fetch_and_and(ptr, n)

#else // __STDC_NO_ATOMICS__

#include <stddef.h>

// 跨语言兼容宏
#if defined(__cplusplus)
#include <atomic>
#define  STD_ std::
#define atomic_value_type_(p, v) decltype((p)->load())(v)
#else // __cplusplus
#include <stdatomic.h>
#define  STD_
#define atomic_value_type_(p, v) typeof(atomic_load(p))(v)
#endif // __cplusplus

// 定义原子类型和操作宏
#define ATOM_INT  STD_ atomic_int
#define ATOM_POINTER STD_ atomic_uintptr_t
#define ATOM_SIZET STD_ atomic_size_t
#define ATOM_ULONG STD_ atomic_ulong
#define ATOM_INIT(ref, v) STD_ atomic_init(ref, v)
#define ATOM_LOAD(ptr) STD_ atomic_load(ptr)
#define ATOM_STORE(ptr, v) STD_ atomic_store(ptr, v)

static inline int
ATOM_CAS(ATOM_INT *ptr, int oval, int nval)
{
    // &oval 的作用是告诉 CAS 函数期望旧值的内存地址
    // 使得函数在操作失败时，能够将原子变量的实际当前值写回这个地址，方便程序进行重试（通常在无锁循环中）
    // 如果只传值，函数将无法返回失败时的实际值。
    return STD_ atomic_compare_exchange_weak(ptr, &oval, nval);
}
//只在当前源文件内部可见。 (static)
//建议编译器进行内联优化。 (inline)
//返回一个整数。 (int)
static inline int
ATOM_CAS_SIZET(STD_ atomic_size_t *ptr, size_t oval, size_t nval) {
	return STD_ atomic_compare_exchange_weak(ptr, &(oval), nval);
}

static inline int
ATOM_CAS_ULONG(STD_ atomic_ulong *ptr, unsigned long oval, unsigned long nval) {
	return STD_ atomic_compare_exchange_weak(ptr, &(oval), nval);
}

static inline int
ATOM_CAS_POINTER(STD_ atomic_uintptr_t *ptr, uintptr_t oval, uintptr_t nval) {
	return STD_ atomic_compare_exchange_weak(ptr, &(oval), nval);
}

#define ATOM_FINC(ptr) STD_ atomic_fetch_add(ptr, atomic_value_type_(ptr,1))
#define ATOM_FDEC(ptr) STD_ atomic_fetch_sub(ptr, atomic_value_type_(ptr, 1))
#define ATOM_FADD(ptr,n) STD_ atomic_fetch_add(ptr, atomic_value_type_(ptr, n))
#define ATOM_FSUB(ptr,n) STD_ atomic_fetch_sub(ptr, atomic_value_type_(ptr, n))
#define ATOM_FAND(ptr,n) STD_ atomic_fetch_and(ptr, atomic_value_type_(ptr, n))

#endif // __STDC_NO_ATOMICS__

#endif // SKYNET_ATOMIC_H