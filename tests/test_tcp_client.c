#include "tcp_client.h"
#include "thread_wrapper.h"
#include "log_module.h"
#include <unistd.h>
#include <stdio.h>

#define RING_MEM_SIZE 256
static uint8_t g_ring_memory[RING_MEM_SIZE];
static block_ringbuf_t g_tcp_rb;
static tcp_client_t g_tcp_cli;

// 业务消费线程：从环形缓冲取出网络收到的数据
static void *data_consume_thread(void *arg)
{
    thread_wrapper_t *thr = (thread_wrapper_t *)arg;
    LOG_INFO("data consume thread start");

    uint8_t r_buf[64];
    while (!thr->want_exit)
    {
        // 阻塞读取
        int ret = block_ringbuf_read_batch(&g_tcp_rb, r_buf, 1);
        if(ret < 0)
        {
            LOG_INFO("ringbuf stop, consumer exit");
            break;
        }
        LOG_INFO("recv from tcp: 0x%02X", r_buf[0]);
        printf("DEBUG RECV: 0x%02X\n", r_buf[0]);
    }
    LOG_INFO("data consume thread exit");
    return NULL;
}

int main(void)
{
    thread_wrapper_t thr_consume;
    LOG_INFO("==== test tcp client start ====");

    // 初始化阻塞环形缓冲
    int ret = block_ringbuf_init(&g_tcp_rb, g_ring_memory, RING_MEM_SIZE);
    if(ret != 0)
    {
        LOG_ERROR("block ringbuf init fail");
        return -1;
    }

    // TCP客户端配置
    tcp_client_cfg_t cfg = {
        .server_ip = "127.0.0.1",
        .server_port = 8080,
        .reconnect_ms = 2000,   // 断开之后2s重连
        .heartbeat_ms = 5000
    };

    ret = tcp_client_init(&g_tcp_cli, &cfg, &g_tcp_rb);
    if(ret != 0)
    {
        LOG_ERROR("tcp client init fail");
        goto out_destroy_rb;
    }

    // 创建数据消费线程
    ret = thread_wrapper_create(&thr_consume, data_consume_thread, &thr_consume);
    if(ret != 0)
    {
        LOG_ERROR("create consume thread fail");
        goto out_destroy_tcp;
    }

    // 启动tcp客户端后台线程
    tcp_client_start(&g_tcp_cli);

    LOG_INFO("tcp client running, test duration 10s");
    sleep(10);

    // 停止流程
    tcp_client_stop(&g_tcp_cli);
    block_ringbuf_wakeup(&g_tcp_rb);  // 唤醒卡在ringbuf读阻塞的消费线程
    thread_wrapper_stop(&thr_consume);
    thread_wrapper_join(&thr_consume);

out_destroy_tcp:
    tcp_client_destroy(&g_tcp_cli);
out_destroy_rb:
    block_ringbuf_destroy(&g_tcp_rb);
    LOG_INFO("==== test tcp client end ====");
    return 0;
}