#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "log_module.h"
#include "sig_handle.h"

/*
 * 测试逻辑：
 * 1. 初始化信号模块、日志模块
 * 2. 开启3个业务线程，分别循环打印不同等级日志
 * 3. Ctrl+C触发优雅退出，退出前刷完所有留在ringbuf里的日志
 */

static void *thread_a(void *arg)
{
    (void)arg;
    int cnt = 0;
    while (!sig_handle_need_exit())
    {
        LOG_DEBUG("thread_a debug cnt = %d", cnt);
        LOG_INFO("thread_a info cnt = %d", cnt);
        cnt++;
        usleep(200000);
    }
    LOG_INFO("thread_a 准备退出");
    return NULL;
}

static void *thread_b(void *arg)
{
    (void)arg;
    int cnt = 0;
    while (!sig_handle_need_exit())
    {
        LOG_WARN("thread_b warn cnt = %d", cnt);
        cnt++;
        usleep(300000);
    }
    LOG_INFO("thread_b 准备退出");
    return NULL;
}

static void *thread_c(void *arg)
{
    (void)arg;
    int cnt = 0;
    while (!sig_handle_need_exit())
    {
        LOG_ERROR("thread_c error cnt = %d", cnt);
        cnt++;
        usleep(500000);
    }
    LOG_INFO("thread_c 准备退出");
    return NULL;
}

int main(void)
{
    pthread_t tid_a, tid_b, tid_c;

    printf("===== log_module 异步日志测试 =====\n");
    printf("提示：Ctrl + C 优雅退出\n");

    sig_handle_init();
    // ringbuf大小 1024，默认日志等级INFO（DEBUG会被过滤掉）
    log_module_init(1024, LOG_LEVEL_INFO);

    pthread_create(&tid_a, NULL, thread_a, NULL);
    pthread_create(&tid_b, NULL, thread_b, NULL);
    pthread_create(&tid_c, NULL, thread_c, NULL);

    pthread_join(tid_a, NULL);
    pthread_join(tid_b, NULL);
    pthread_join(tid_c, NULL);

    log_module_deinit();
    sig_handle_deinit();
    printf("日志测试全部退出完成\n");
    return 0;
}