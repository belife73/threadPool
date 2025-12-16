# C 语言轻量级线程池 (Thread Pool) 技术文档

## 1. 项目概述

本项目实现了一个基于 C 语言（支持 C11 标准）的高性能、轻量级线程池。该线程池采用**生产者-消费者模型**，旨在解决高并发场景下的任务调度问题。项目核心包含跨平台的原子操作封装、自旋锁实现以及基于条件变量的任务队列管理。

**主要特性：**

- **混合锁机制**：使用自旋锁（Spinlock）保护任务队列的入队/出队操作（短临界区），使用互斥锁（Mutex）+ 条件变量（Cond）管理线程的休眠与唤醒（长等待）。
- **跨平台原子性**：通过 `atomic.h` 封装了 C11 `stdatomic` 和 GCC `__sync` 内置函数，兼容不同编译器环境。
- **动态内存管理**：任务节点动态分配，支持无界队列（受限于内存）。
- **优雅退出**：支持非阻塞的终止通知 (`terminate`) 和阻塞等待回收 (`waitdone`)。

------

## 2. 框架与架构设计

### 2.1 核心组件

系统主要由以下三个模块组成：

1. **任务队列 (Task Queue)**
   - **结构**：单向链表。
   - **并发控制**：
     - `head`/`tail` 指针的修改使用 **Spinlock**（因为链表操作极快，自旋锁效率高于互斥锁）。
     - 线程的阻塞（当队列为空时）使用 **Mutex + Condition Variable**，避免空转浪费 CPU。
   - **入队策略**：使用二级指针技巧 (`void **tail`) 快速追加节点。
2. **工作线程 (Worker Threads)**
   - 线程池初始化时创建指定数量的线程。
   - 每个线程执行 `__thrdpool_worker` 循环：
     - 尝试从队列获取任务。
     - 若队列为空，进入条件变量等待 (`pthread_cond_wait`)。
     - 获取任务后执行回调函数 `handler_pt`。
3. **同步原语层 (Synchronization Primitives)**
   - `spinlock.h`：根据环境定义自旋锁。若支持 C11，使用 `atomic_flag`；否则回退到 GCC 原子操作；亦可宏定义强制使用 `pthread_mutex`。
   - `atomic.h`：统一了原子操作接口（CAS, Load, Store, Fetch-Add），屏蔽底层差异。

### 2.2 逻辑流程图

1. **初始化**: `thread_pool_create` -> 创建 TaskQueue -> 创建 Worker Threads。
2. **生产任务**: `thread_pool_post` -> 封装 Task -> Spinlock 加锁 -> 入队 -> Spinlock 解锁 -> `pthread_cond_signal` 唤醒工作线程。
3. **消费任务**: Worker 唤醒 -> Spinlock 加锁 -> 取出 Head -> Spinlock 解锁 -> 执行任务 -> 销毁 Task 节点。

------

## 3. API 接口说明

在 `thread_pool.h` 中定义了对外接口：

| **函数原型**                                                 | **说明**                   | **参数与返回值**                                             |
| ------------------------------------------------------------ | -------------------------- | ------------------------------------------------------------ |
| `threadpool_t *thread_pool_create(int thread_count)`         | 创建并初始化线程池         | **thread_count**: 线程数量 **返回**: 线程池句柄，失败返回 NULL |
| `int thread_pool_post(threadpool_t *pool, handler_pt func, void *arg)` | 向线程池投递任务           | **pool**: 句柄 **func**: 任务回调函数 **arg**: 上下文参数 **返回**: 0 成功, -1 失败 |
| `void thread_pool_terminate(threadpool_t * pool)`            | 通知线程池停止（非阻塞）   | **pool**: 句柄 设置退出标志，唤醒所有阻塞线程，不再接收新任务 |
| `void thread_pool_waitdone(threadpool_t *pool)`              | 等待所有线程结束并销毁资源 | **pool**: 句柄 阻塞直到所有工作线程退出，释放队列和池的内存  |

------

## 4. 构建与编译指南

项目使用 CMake 进行构建，支持 C 和 C++ 代码，编译为动态库供业务链接。

### 4.1 构建步骤

```bash
# 1. 进入项目根目录
cd /root/project

# 2. 创建构建目录
mkdir -p build
cd build

# 3. 运行 CMake 生成构建文件
cmake ..

# 4. 编译
make

# 5. 运行测试（可选）
./main
./thrdpool_test
```

### 4.2 生成的文件

- **库文件**: `libthrdpool.so` - 线程池动态库
- **测试程序**:
  - `main` - C 语言基础功能测试
  - `thrdpool_test` - C++ 性能测试
  - `task_queue_test` - C++ 单元测试（需要 GTest）

------

## 5. 测试文档

本项目提供了三个层级的测试：功能测试、单元测试和性能基准测试。

### 5.1 功能集成测试 (`main`)

**目的**：验证线程池的基本创建、任务执行和销毁流程是否没有死锁或崩溃。

- **测试逻辑**：
  1. 创建 8 个线程的池。
  2. 主线程循环投递任务（最多 2000 个）。
  3. 任务函数 `do_task` 对全局变量 `done` 进行加锁累加。
  4. 当 `done` 达到 1000 时，调用 `thread_pool_terminate`。
  5. 主线程调用 `thread_pool_waitdone` 回收资源。

- **运行**：
  ```bash
  ./main
  ```

### 5.2 单元测试 (`task_queue_test`)

**目的**：针对内部数据结构 `task_queue` 进行白盒测试，重点验证内存泄漏和队列逻辑。需要依赖 **Google Test (gtest)** 框架。

- **测试用例**：
  - `TEST(task_queue, normal)`: 测试队列的创建、入队出队操作和内存管理。

- **运行**：
  ```bash
  ./task_queue_test
  ```

### 5.3 性能/基准测试 (`thrdpool_test`)

**目的**：评估线程池在“多生产者-多消费者”模型下的吞吐量（QPS）。

- **测试场景**：
  - **生产者**：启动 `n` 个 `std::thread` 线程，每个线程疯狂投递任务。
  - **消费者**：线程池内部的 Worker 线程。
  - **任务**：简单的原子累加操作 (`JustTask`)，模拟高频短任务。
  - **指标**：计算处理 1,000,000 * n 个任务所需的总耗时，输出“每秒执行次数”。

- **代码配置**：
  - 默认测试：`test_thrdpool(4, 4)` (4个生产者线程，4个线程池Worker)。
  - 可调整 `main` 函数中的参数进行压力测试。

- **运行**：
  ```bash
  ./thrdpool_test
  ```

- **结果解读示例**：
  ```
  2218446 2217641 used:805 exec per sec:4.96894e+06
  ```
  *注意：若 `exec per sec` 数值较低，可能原因是锁竞争激烈或任务本身过于简单导致调度开销占比过大。*

------

## 6. 常见问题 (FAQ)

1. **为什么要自己实现 Spinlock？**
   - `pthread_mutex` 在某些老旧内核或特定场景下上下文切换开销较大。对于队列指针的修改通常只需几条指令，使用自旋锁（用户态空转）能避免切入内核态，显著提升高并发下的入队/出队性能。

2. **如何避免虚假唤醒？**
   - 在 `thread_pool.c` 的 `__get_task` 函数中，使用了 `while` 循环检查队列状态：
     ```C
     while ((task = __pop_task(queue)) == NULL) {
         // ... wait ...
     }
     ```
     这是使用条件变量的标准范式。

3. **内存泄漏检测**
   - 建议使用 Valgrind 运行 `main` 或 `task_queue_test`：
     ```bash
     valgrind --leak-check=full ./main
     ```
   - `thread_pool_waitdone` 函数中包含了对剩余任务和线程资源的清理逻辑，确保正常退出时无泄漏。

4. **如何添加自定义任务？**
   - 定义一个符合 `handler_pt` 类型的回调函数，例如：
     ```C
     void my_task(void *arg) {
         // 任务逻辑
     }
     ```
   - 使用 `thread_pool_post(pool, my_task, arg)` 投递任务。

------

## 7. 项目结构

```
/root/project/
├── CMakeLists.txt      # CMake 构建配置文件
├── include/            # 头文件目录
│   ├── atomic.h        # 原子操作封装
│   ├── spinlock.h      # 自旋锁实现
│   └── thread_pool.h   # 线程池对外接口
├── src/                # 源代码目录
│   └── thread_pool.c   # 线程池核心实现
├── test/               # 测试程序目录
│   ├── main.c          # C 功能测试
│   ├── thrdpool_test.cc # C++ 性能测试
│   └── task_queue.cc   # C++ 单元测试
└── README.md           # 项目文档
```

------

## 8. 许可证

本项目采用 MIT 许可证，详情请查看 LICENSE 文件。