#include "thread_wrapper.h"
#include "log_module.h"
#include "sig_handle.h"
#include <unistd.h>
#include <stdio.h>

/*
 * 测试线程的业务函数：每隔500ms打印一次日志，循环检测 want_exit
 */
static void *test_work_thread(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    int cnt = 0;

    while (true)
    {
        // 重点：每次循环先判断外部的退出标记
        if (thr->want_exit)
        {
            LOG_INFO("work thread receive exit flag, ready to quit");
            break;
        }

        LOG_INFO("work thread running, count = %d", cnt++);
        usleep(500 * 1000);  // 休眠500ms
    }

    LOG_INFO("work thread exit normally");
    return NULL;
}

int main(void)
{
    thread_wrapper_t thr;
    int ret;
    log_module_init(32,LOG_LEVEL_INFO);
    LOG_INFO("==== test thread wrapper start ====");
    // 1. 创建启动线程
    ret = thread_wrapper_create(&thr, test_work_thread, &thr);
    if (ret != 0)
    {
        printf("create thread failed!\n");
        LOG_ERROR("create test thread fail");
        return -1;
    }

    // 主线程等待2.5秒，让子线程跑一会儿
    sleep(2);

    // 2. 发停止信号：设置退出标记
    thread_wrapper_stop(&thr);

    // 3. join 阻塞等待子线程安全退出、回收资源
    ret = thread_wrapper_join(&thr);
    if (ret != 0)
    {
        LOG_ERROR("join thread fail");
    }

    // 4. 清理
    thread_wrapper_destroy(&thr);
    LOG_INFO("==== test thread wrapper end ====");

    return 0;
}