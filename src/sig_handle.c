#include "sig_handle.h"
#include <stdatomic.h>
#include <stdio.h>

// 原子标记：多线程安全，用来标记是否收到退出信号
static atomic_bool g_need_exit = ATOMIC_VAR_INIT(false);
static struct sigaction old_sigint;
static struct sigaction old_sigterm;

static void sig_exit_handler(int sig)
{
    (void)sig;
    atomic_store(&g_need_exit, true);
    printf("\n[sig] 收到退出信号，准备优雅退出...\n");
}

int sig_handle_init(void)
{
    struct sigaction sa;

    sa.sa_handler = sig_exit_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // 注册 SIGINT (Ctrl + C)
    if (sigaction(SIGINT, &sa, &old_sigint) < 0)
    {
        perror("sigaction SIGINT failed");
        return -1;
    }
    // 注册 SIGTERM (kill 进程默认信号)
    if (sigaction(SIGTERM, &sa, &old_sigterm) < 0)
    {
        perror("sigaction SIGTERM failed");
        return -1;
    }
    return 0;
}

bool sig_handle_need_exit(void)
{
    return atomic_load(&g_need_exit);
}

void sig_handle_deinit(void)
{
    // 恢复原来的信号处理函数
    sigaction(SIGINT, &old_sigint, NULL);
    sigaction(SIGTERM, &old_sigterm, NULL);
}
