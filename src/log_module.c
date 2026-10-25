#include "log_module.h"
#include "ringbuf.h"
#include "sig_handle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

#define LOG_MAX_MSG_LEN 256

static ringbuf_t *g_log_rb = NULL;
static log_level_t g_log_level = LOG_LEVEL_INFO;
static pthread_t g_log_dump_tid;
static bool g_thread_running = false;

static const char *level_str[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

// 日志刷写线程：从ringbuf取出日志，输出到stdout
static void *log_dump_thread(void *arg)
{
    (void)arg;
    uint8_t tmp_buf[LOG_MAX_MSG_LEN];
    uint32_t read_len;

    while (!sig_handle_need_exit())
    {
        read_len = ringbuf_read(g_log_rb, tmp_buf, LOG_MAX_MSG_LEN - 1);
        if (read_len > 0)
        {
            tmp_buf[read_len] = '\0';
            printf("%s", tmp_buf);
            fflush(stdout);
        }
        else
        {
            usleep(50000);
        }
    }

    // 收到退出信号，把缓冲区残留日志全部刷完再退出
    while ((read_len = ringbuf_read(g_log_rb, tmp_buf, LOG_MAX_MSG_LEN - 1)) > 0)
    {
        tmp_buf[read_len] = '\0';
        printf("%s", tmp_buf);
        fflush(stdout);
    }
    printf("[log] 日志刷写线程退出\n");
    return NULL;
}

int log_module_init(uint32_t buf_size, log_level_t level)
{
    if (g_log_rb != NULL)
    {
        // 已经初始化过
        return 0;
    }
    g_log_rb = ringbuf_create(buf_size);
    if (g_log_rb == NULL)
    {
        return -1;
    }
    g_log_level = level;
    g_thread_running = true;

    if (pthread_create(&g_log_dump_tid, NULL, log_dump_thread, NULL) != 0)
    {
        ringbuf_destroy(g_log_rb);
        g_log_rb = NULL;
        return -1;
    }
    return 0;
}

void log_module_deinit(void)
{
    if (g_log_rb == NULL)
        return;

    // 等待刷写线程结束（sig标记会让它刷完剩余日志）
    pthread_join(g_log_dump_tid, NULL);
    ringbuf_destroy(g_log_rb);
    g_log_rb = NULL;
    g_thread_running = false;
}

void log_module_set_level(log_level_t level)
{
    g_log_level = level;
}

void log_print(log_level_t level, const char *file, int line, const char *fmt, ...)
{
    if (level < g_log_level || g_log_rb == NULL)
    {
        return;
    }

    char msg_buf[LOG_MAX_MSG_LEN];
    va_list ap;
    va_start(ap, fmt);
    int len = snprintf(msg_buf, LOG_MAX_MSG_LEN,
                        "[%s] %s:%d - ", level_str[level], file, line);
    if (len > 0 && len < LOG_MAX_MSG_LEN)
    {
        vsnprintf(msg_buf + len, LOG_MAX_MSG_LEN - len, fmt, ap);
    }
    va_end(ap);

    strcat(msg_buf, "\n");
    ringbuf_write(g_log_rb, (uint8_t *)msg_buf, strlen(msg_buf));
    //printf("%s",msg_buf);
}