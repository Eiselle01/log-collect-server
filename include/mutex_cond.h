#ifndef MUTEX_COND_H
#define MUTEX_COND_H

#include <pthread.h>
#include <stdbool.h>

/**
 * @brief 互斥锁 + 条件变量封装结构体
 * 典型生产者消费者模型配套
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool            init_ok;  // 标记是否初始化成功，防止重复销毁
} mutex_cond_t;

/**
 * @brief 初始化锁+条件变量
 * @param mc 句柄指针
 * @return 0成功，<0失败
 */
int mutex_cond_init(mutex_cond_t *mc);

/**
 * @brief 加锁
 */
int mutex_cond_lock(mutex_cond_t *mc);

/**
 * @brief 解锁
 */
int mutex_cond_unlock(mutex_cond_t *mc);

/**
 * @brief 等待条件（会自动解锁mutex，被唤醒后重新加锁）
 * 注意：调用这个之前必须已经lock！
 */
int mutex_cond_wait(mutex_cond_t *mc);

/**
 * @brief 唤醒一个等待在条件上的线程
 */
int mutex_cond_signal(mutex_cond_t *mc);

/**
 * @brief 唤醒全部等待的线程
 */
int mutex_cond_broadcast(mutex_cond_t *mc);

/**
 * @brief 销毁锁和条件变量
 */
void mutex_cond_destroy(mutex_cond_t *mc);

#endif