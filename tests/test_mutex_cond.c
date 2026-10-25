#include "mutex_cond.h"
#include "thread_wrapper.h"
#include "log_module.h"
#include <unistd.h>
#include <stdio.h>

// 共享全局数据（多线程访问，必须锁保护）
static mutex_cond_t g_mc;
static int g_data_ready = 0;   // 数据就绪标记

// 消费者线程：等待生产者产生数据
static void *consumer_thread(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    LOG_INFO("consumer thread start");

    while (!thr->want_exit)
    {
        mutex_cond_lock(&g_mc);

        // ✅ 重点：while 而不是 if，处理【虚假唤醒】
        while (!g_data_ready && !thr->want_exit)
        {
            LOG_INFO("consumer: waiting for data...");
            mutex_cond_wait(&g_mc);
        }

        if (g_data_ready)
        {
            LOG_INFO("consumer: get data, process it");
            g_data_ready = 0;  // 消费完成，清空标记
        }

        mutex_cond_unlock(&g_mc);
        sleep(1);
    }

    LOG_INFO("consumer thread exit");
    return NULL;
}

// 生产者线程：每隔2s产生一份数据
static void *producer_thread(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    LOG_INFO("producer thread start");

    while (!thr->want_exit)
    {
        sleep(2);

        mutex_cond_lock(&g_mc);
        g_data_ready = 1;
        LOG_INFO("producer: data ready, send signal");
        mutex_cond_signal(&g_mc);  // 唤醒等待的消费者
        mutex_cond_unlock(&g_mc);
    }

    LOG_INFO("producer thread exit");
    return NULL;
}

int main(void)
{
    thread_wrapper_t thr_prod, thr_cons;
    int ret;

    LOG_INFO("==== test mutex cond producer-consumer start ====");

    // 初始化锁+条件变量
    ret = mutex_cond_init(&g_mc);
    if (ret != 0)
    {
        LOG_ERROR("mutex cond init fail");
        return -1;
    }

    // 创建生产者、消费者线程
    ret = thread_wrapper_create(&thr_prod, producer_thread, &thr_prod);
    if (ret != 0)
    {
        LOG_ERROR("create producer fail");
        goto out;
    }
    ret = thread_wrapper_create(&thr_cons, consumer_thread, &thr_cons);
    if (ret != 0)
    {
        LOG_ERROR("create consumer fail");
        goto out;
    }

    // 主线程等待6秒，让生产消费跑几轮
    sleep(6);

    // 优雅停止两个线程
    thread_wrapper_stop(&thr_prod);
    thread_wrapper_stop(&thr_cons);
    // 发广播唤醒卡在wait里的消费者，不然会阻塞在join
    mutex_cond_lock(&g_mc);
    mutex_cond_broadcast(&g_mc);
    mutex_cond_unlock(&g_mc);

    thread_wrapper_join(&thr_prod);
    thread_wrapper_join(&thr_cons);

out:
    mutex_cond_destroy(&g_mc);
    LOG_INFO("==== test mutex cond end ====");
    return 0;
}