#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "ringbuf.h"
#include "sig_handle.h"

/*
 * 测试方案：
 * 一个生产者线程不断往环里写数据
 * 一个消费者线程不断从环里读数据、校验内容
 * 配合我们之前的sig_handle模块，Ctrl+C可以优雅退出
 */

#define RB_BUF_SIZE 64
#define TEST_DATA_BASE 0x10

static ringbuf_t *g_rb = NULL;

// 生产者：递增写入 0x10,0x11,0x12...
static void *producer_task(void *arg)
{
    (void)arg;
    uint8_t w_data = TEST_DATA_BASE;
    uint32_t ret;

    while (!sig_handle_need_exit())
    {
        ret = ringbuf_write(g_rb, &w_data, 1);
        if (ret == 1)
        {
            printf("[producer] write: 0x%02x\n", w_data);
            w_data++;
        }
        else
        {
            // 缓冲区满，短暂让出CPU
            usleep(10000);
        }
    }
    printf("[producer] 收到退出标记，生产者线程退出\n");
    return NULL;
}

// 消费者：读出数据，校验是否连续
static void *consumer_task(void *arg)
{
    (void)arg;
    uint8_t r_data;
    uint8_t expect = TEST_DATA_BASE;
    uint32_t ret;

    while (!sig_handle_need_exit())
    {
        ret = ringbuf_read(g_rb, &r_data, 1);
        if (ret == 1)
        {
            if (r_data != expect)
            {
                printf("[consumer ERROR] expect 0x%02x, get 0x%02x\n", expect, r_data);
            }
            else
            {
                printf("[consumer] read ok: 0x%02x\n", r_data);
            }
            expect++;
        }
        else
        {
            usleep(10000);
        }
    }
    printf("[consumer] 收到退出标记，消费者线程退出\n");
    return NULL;
}

int main(void)
{
    pthread_t tid_prod, tid_cons;
    int ret;

    printf("===== ringbuf 单元测试（生产者-消费者）=====\n");
    printf("提示：Ctrl + C 优雅退出\n");

    sig_handle_init();

    g_rb = ringbuf_create(RB_BUF_SIZE);
    if (g_rb == NULL)
    {
        fprintf(stderr, "ringbuf create failed\n");
        sig_handle_deinit();
        return -1;
    }

    pthread_create(&tid_prod, NULL, producer_task, NULL);
    pthread_create(&tid_cons, NULL, consumer_task, NULL);

    pthread_join(tid_prod, NULL);
    pthread_join(tid_cons, NULL);

    ringbuf_destroy(g_rb);
    sig_handle_deinit();
    printf("ringbuf 测试结束\n");
    return 0;
}