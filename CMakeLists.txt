cmake_minimum_required(VERSION 3.16)
project(embedded_component_lib C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 头文件搜索路径
include_directories(include)

# 收集所有库源码
set(LIB_SRC
    src/sig_handle.c
    src/ringbuf.c
    src/log_module.c
    src/thread_wrapper.c
    src/mutex_cond.c  
)

# 编译成静态库 libembedded_comp.a
add_library(embedded_comp STATIC ${LIB_SRC})

# 链接pthread给静态库
target_link_libraries(embedded_comp PUBLIC pthread)

# ========== 各个测试程序 ==========
# 信号测试
add_executable(test_sig tests/test_sig.c)
target_link_libraries(test_sig embedded_comp)

# 环形缓冲区测试
add_executable(test_ringbuf tests/test_ringbuf.c)
target_link_libraries(test_ringbuf embedded_comp)

# 日志模块测试
add_executable(test_log tests/test_log.c)
target_link_libraries(test_log embedded_comp)

# 新增：线程封装测试
add_executable(test_thread_wrapper tests/test_thread_wrapper.c)
target_link_libraries(test_thread_wrapper embedded_comp)

#
add_executable(test_mutex_cond tests/test_mutex_cond.c)
target_link_libraries(test_mutex_cond embedded_comp)