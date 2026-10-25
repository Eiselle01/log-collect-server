#ifndef THREAD_WRAPPER_H
#define THREAD_WRAPPER_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 线程入口函数类型，和pthread入口对齐
 * @param arg 传入线程的私有参数
 * @return 线程退出返回值
 */
typedef void *(*thread_entry_t)(void *arg);

/**
 * @brief 线程管理结构体
 * 封装线程句柄、运行标记、退出标记
 * 用原子bool，避免多线程访问竞争（后面解释原子操作）
 */
typedef struct {
    pthread_t       tid;          // 系统线程id
    thread_entry_t  entry;       // 线程执行函数
    void            *arg;         // 传给线程的参数
    volatile bool   is_running;   // 线程是否正在运行
    volatile bool   want_exit;    // 外部请求线程退出标记
} thread_wrapper_t;

/**
 * @brief 创建并启动线程
 * @param thr 线程管理对象指针
 * @param entry 线程业务函数
 * @param arg 业务参数
 * @return 0成功，负数失败
 */
int thread_wrapper_create(thread_wrapper_t *thr, thread_entry_t entry, void *arg);

/**
 * @brief 请求线程优雅退出，设置退出标记
 * @param thr 线程管理对象指针
 */
void thread_wrapper_stop(thread_wrapper_t *thr);

/**
 * @brief 阻塞等待线程结束（join），回收资源
 * @param thr 线程管理对象指针
 * @return 0成功
 */
int thread_wrapper_join(thread_wrapper_t *thr);

/**
 * @brief 销毁线程包装实例，清理标记
 */
void thread_wrapper_destroy(thread_wrapper_t *thr);

#endif