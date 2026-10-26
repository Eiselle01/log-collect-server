#include "tcp_client.h"
#include "thread_wrapper.h"
#include "log_module.h"
#include "block_ringbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

// 静态线程入口，TCP后台工作线程
static void *tcp_client_work_thread(void *arg);

int tcp_client_init(tcp_client_t *client, const tcp_client_cfg_t *cfg, block_ringbuf_t *recv_rb)
{
    if (client == NULL || cfg == NULL || recv_rb == NULL)
    {
        LOG_ERROR("tcp_client_init invalid param");
        return -1;
    }
    memset(client, 0, sizeof(tcp_client_t));

    strncpy(client->cfg.server_ip, cfg->server_ip, sizeof(client->cfg.server_ip)-1);
    client->cfg.server_port    = cfg->server_port;
    client->cfg.reconnect_ms   = cfg->reconnect_ms;
    client->cfg.heartbeat_ms   = cfg->heartbeat_ms;
    client->sockfd             = -1;
    client->connected          = false;
    client->stop_request       = false;
    client->recv_rb            = recv_rb;

    // 网络开发经典坑：屏蔽SIGPIPE，防止write时对端断开直接杀死进程
    signal(SIGPIPE, SIG_IGN);

    LOG_INFO("tcp client init ok, server:%s:%d", client->cfg.server_ip, client->cfg.server_port);
    return 0;
}

static void *tcp_client_work_thread(void *arg)
{
    tcp_client_t *client = (tcp_client_t *)arg;
    struct sockaddr_in serv_addr;
    int ret;

    LOG_INFO("tcp client work thread start");

    while (!client->stop_request)
    {
        // 如果当前未连接，尝试连接服务器
        if (!client->connected)
        {
            if (client->sockfd > 0)
            {
                close(client->sockfd);
                client->sockfd = -1;
            }

            client->sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (client->sockfd < 0)
            {
                LOG_ERROR("socket create fail, errno=%d", errno);
                usleep(client->cfg.reconnect_ms * 1000);
                continue;
            }

            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(client->cfg.server_port);
            ret = inet_pton(AF_INET, client->cfg.server_ip, &serv_addr.sin_addr);
            if (ret <= 0)
            {
                LOG_ERROR("inet_pton invalid ip");
                close(client->sockfd);
                client->sockfd = -1;
                usleep(client->cfg.reconnect_ms * 1000);
                continue;
            }

            ret = connect(client->sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            if (ret < 0)
            {
                LOG_WARN("connect server fail, will reconnect later");
                close(client->sockfd);
                client->sockfd = -1;
                usleep(client->cfg.reconnect_ms * 1000);
                continue;
            }

            client->connected = true;
            LOG_INFO("tcp connect success!");
        }

        // 连接成功，循环接收数据
        uint8_t tmp_buf[256];
        ret = read(client->sockfd, tmp_buf, sizeof(tmp_buf));
        if (ret > 0)
        {
            // 读到数据直接写入阻塞环形缓冲区
            block_ringbuf_write_batch(client->recv_rb, tmp_buf, ret);
        }
        else if (ret == 0)
        {
            // 对端正常关闭连接
            LOG_WARN("server closed connection");
            client->connected = false;
        }
        else
        {
            // read出错
            LOG_ERROR("read sock error, errno=%d", errno);
            client->connected = false;
        }
    }

    LOG_INFO("tcp client work thread exit");
    return NULL;
}

int tcp_client_start(tcp_client_t *client)
{
    // 这里你可以用 thread_wrapper 包装启动线程
    // 本版本先预留，后面可以直接接入我们的thread_wrapper
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, tcp_client_work_thread, client);
    if(ret != 0)
    {
        LOG_ERROR("pthread create failed");
        return -1;
    }
    pthread_detach(tid);
    return 0;
}

int tcp_client_send(tcp_client_t *client, const uint8_t *buf, uint32_t len)
{
    if(client == NULL || !client->connected || client->sockfd < 0)
    {
        LOG_WARN("tcp not connected, send drop");
        return -1;
    }
    ssize_t s = write(client->sockfd, buf, len);
    if(s < 0)
    {
        LOG_ERROR("tcp write fail");
        client->connected = false;
        return -1;
    }
    return (int)s;
}

void tcp_client_stop(tcp_client_t *client)
{
    if(client == NULL)
        return;
    client->stop_request = true;
    client->connected = false;
    if(client->sockfd > 0)
    {
        close(client->sockfd);
        client->sockfd = -1;
    }
    LOG_INFO("tcp client stop request set");
}

void tcp_client_destroy(tcp_client_t *client)
{
    if(client == NULL)
        return;
    tcp_client_stop(client);
    memset(client,0,sizeof(tcp_client_t));
    LOG_INFO("tcp client destroy");
}