#include "thread_wrapper.h"
#include "log_module.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 内部包装函数：真正传给pthread的入口
 * 作用：先把我们自定义的thread_wrapper_t传进来，再调用用户写的业务入口
 */
static void *thread_inner_entry(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    if (thr == NULL || thr->entry == NULL)
    {
        LOG_ERROR("thread inner entry invalid param");
        return NULL;
    }

    // 标记线程开始运行
    thr->is_running = true;
    LOG_INFO("thread start run");

    // 调用用户的业务线程函数
    void *ret = thr->entry(thr->arg);

    // 业务函数返回，线程即将退出
    thr->is_running = false;
    LOG_INFO("thread entry exit");

    return ret;
}

int thread_wrapper_create(thread_wrapper_t *thr, thread_entry_t entry, void *arg)
{
    if (thr == NULL || entry == NULL)
    {
        LOG_ERROR("thread_wrapper_create null pointer");
        return -1;
    }
    // 先清零结构体，初始化标记
    memset(thr, 0, sizeof(thread_wrapper_t));
    thr->entry = entry;
    thr->arg   = arg;
    thr->want_exit = false;
    thr->is_running = false;

    // 创建pthread线程，绑定内部入口函数
    int ret = pthread_create(&thr->tid, NULL, thread_inner_entry, thr);
    if (ret != 0)
    {
        LOG_ERROR("pthread_create failed, ret:%d", ret);
        return -2;
    }
    LOG_INFO("thread create success");
    return 0;
}

void thread_wrapper_stop(thread_wrapper_t *thr)
{
    if (thr == NULL)
    {
        return;
    }
    // 设置退出标志，通知业务线程退出
    thr->want_exit = true;
    LOG_INFO("send thread stop request");
}

int thread_wrapper_join(thread_wrapper_t *thr)
{
    if (thr == NULL)
    {
        LOG_ERROR("thread_wrapper_join null");
        return -1;
    }
    if (thr->tid == 0)
    {
        LOG_WARN("thread tid is 0, no thread to join");
        return 0;
    }
    void *res = NULL;
    int ret = pthread_join(thr->tid, &res);
    if (ret != 0)
    {
        LOG_ERROR("pthread_join failed ret:%d", ret);
        return -2;
    }
    LOG_INFO("thread join ok");
    return 0;
}

void thread_wrapper_destroy(thread_wrapper_t *thr)
{
    if (thr == NULL)
        return;
    // 清空信息
    thr->tid = 0;
    thr->entry = NULL;
    thr->arg = NULL;
    thr->is_running = false;
    thr->want_exit = false;
}