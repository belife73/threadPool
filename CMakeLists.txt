# 设置项目名称和最低CMake版本
cmake_minimum_required(VERSION 3.10)
project(thrdpool C CXX)

# 设置构建类型，如果是Debug则开启更多警告
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")
endif()

# 设置公共包含路径
include_directories(include)

# -------------------- 库文件 (libthrdpool.so) --------------------

# 定义线程池的源文件
set(THRDPOOL_SRCS
    src/thread_pool.c
)

# 添加线程池共享库
# 链接Pthreads库是必须的，因为用到了pthread_t, pthread_mutex_t等
add_library(thrdpool SHARED ${THRDPOOL_SRCS})

# 设置链接选项：
# -fPIC (Position Independent Code) 是生成共享库所必需的
# -lpthread 是必须链接的库
target_compile_options(thrdpool PRIVATE -fPIC)
target_link_libraries(thrdpool PRIVATE pthread)

# 设置库的安装目录 (可选)
install(TARGETS thrdpool DESTINATION lib)
install(FILES include/thread_pool.h DESTINATION include)

# -------------------- 测试程序 --------------------

# 1. C 语言基础测试 (main.c)
add_executable(main test/main.c)
target_link_libraries(main PRIVATE thrdpool pthread)

# 2. C++ 性能测试 (thrdpool_test.cc)
# C++ 程序需要链接 thrdpool 库
add_executable(thrdpool_test test/thrdpool_test.cc)
target_link_libraries(thrdpool_test PRIVATE thrdpool pthread)

# 3. C++ 任务队列测试 (task_queue.cc) - 需要 GTest
# 查找 GTest 包，如果找不到，跳过这个测试
find_package(GTest QUIET)

if (GTEST_FOUND)
    add_executable(task_queue_test test/task_queue.cc)
    target_link_libraries(task_queue_test PRIVATE thrdpool GTest::gtest_main pthread)
else()
    message(WARNING "GTest not found, skipping task_queue_test")
endif()