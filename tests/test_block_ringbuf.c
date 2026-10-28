#include "block_ringbuf.h"
#include "thread_wrapper.h"
#include "log_module.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define RING_BUF_SIZE  128
// 底层内存
static uint8_t g_ring_buf_mem[RING_BUF_SIZE];
static block_ringbuf_t g_blk_rb;

// 生产者：循环批量写入数据
static void *producer_thread(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    LOG_INFO("block ringbuf producer start");

    uint8_t w_buf[16];
    int seq = 0;
    while (!thr->want_exit)
    {
        // 填充测试数据
        for(int i = 0; i < sizeof(w_buf); i++)
        {
            w_buf[i] = seq++;
        }
        LOG_INFO("producer write batch, seq start = %d", seq - 16);

        // 批量写入16字节
        block_ringbuf_write_batch(&g_blk_rb, w_buf, sizeof(w_buf));
        usleep(300 * 1000);
    }

    LOG_INFO("producer thread exit");
    return NULL;
}

// 消费者：批量读取并校验数据
static void *consumer_thread(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    LOG_INFO("block ringbuf consumer start");

    uint8_t r_buf[16];
    int expect_seq = 0;
    while (!thr->want_exit)
    {
        block_ringbuf_read_batch(&g_blk_rb, r_buf, sizeof(r_buf));

        // 校验读出的数据是否符合预期
        bool ok = true;
        for(int i = 0; i < sizeof(r_buf); i++)
        {
            if(r_buf[i] != (uint8_t)expect_seq)
            {
                ok = false;
                break;
            }
            expect_seq++;
        }

        if(ok)
        {
            LOG_INFO("consumer read batch OK, expect seq=%d", expect_seq);
            printf("DEBUG RECV: 0x%02X\n", r_buf[0]);
        }
        else
        {
            LOG_ERROR("consumer data check fail!");
        }
    }

    LOG_INFO("consumer thread exit");
    return NULL;
}

int main(void)
{
    thread_wrapper_t thr_prod, thr_cons;
    int ret;

    LOG_INFO("==== test block ringbuf batch start ====");

    // 初始化阻塞环形缓冲区
    ret = block_ringbuf_init(&g_blk_rb, g_ring_buf_mem, RING_BUF_SIZE);
    if(ret != 0)
    {
        LOG_ERROR("block ringbuf init fail");
        return -1;
    }

    // 创建生产者、消费者线程
    ret = thread_wrapper_create(&thr_prod, producer_thread, &thr_prod);
    if(ret != 0)
    {
        LOG_ERROR("create producer thread fail");
        goto out;
    }
    ret = thread_wrapper_create(&thr_cons, consumer_thread, &thr_cons);
    if(ret != 0)
    {
        LOG_ERROR("create consumer thread fail");
        goto out;
    }

    // 让生产消费跑 5秒
        // 让生产消费跑 5秒
    sleep(5);

    // 优雅停止线程
    thread_wrapper_stop(&thr_prod);
    thread_wrapper_stop(&thr_cons);
    block_ringbuf_wakeup(&g_blk_rb);  // ✅ 新增：唤醒阻塞在ringbuf上的线程

    thread_wrapper_join(&thr_prod);
    thread_wrapper_join(&thr_cons);
out:
    block_ringbuf_destroy(&g_blk_rb);
    LOG_INFO("==== test block ringbuf test end ====");
    return 0;
}