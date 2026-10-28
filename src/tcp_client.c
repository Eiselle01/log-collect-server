#include "tcp_client.h"
#include "thread_wrapper.h"
#include "log_module.h"
#include "time_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>

#define SEND_RB_SIZE 1024  // 发送环形缓冲区大小

static void *tcp_client_work_thread(void *arg);
static bool set_fd_nonblock(int fd);
static bool check_connect_result(int fd, uint32_t timeout_ms);

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
    client->cfg.link_timeout_ms= cfg->link_timeout_ms;

    client->sockfd             = -1;
    client->connected          = false;
    client->stop_request       = false;
    client->recv_rb            = recv_rb;

    //原始内存，存放要发送的数据
     uint8_t send_buf_mem[1024];
    // ✅ 初始化发送环形缓冲区
   
    block_ringbuf_init(&client->send_rb, send_buf_mem,SEND_RB_SIZE);

    signal(SIGPIPE, SIG_IGN);
    LOG_INFO("tcp client init ok, server:%s:%d", client->cfg.server_ip, client->cfg.server_port);
    return 0;
}

static bool set_fd_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

static bool check_connect_result(int fd, uint32_t timeout_ms)
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (ret <= 0)
    {
        LOG_WARN("connect select timeout or error");
        return false;
    }
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0)
    {
        LOG_ERROR("connect fail, err:%d", err);
        return false;
    }
    return true;
}

static void *tcp_client_work_thread(void *arg)
{
    tcp_client_t *client = (tcp_client_t *)arg;
    struct sockaddr_in serv_addr;
    int ret;
    uint8_t tmp_buf[256];
    uint8_t send_buf[256];
    LOG_INFO("tcp client work thread start");

    while (!client->stop_request)
    {
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
            if (!set_fd_nonblock(client->sockfd))
            {
                LOG_ERROR("set nonblock fail");
                close(client->sockfd);
                client->sockfd = -1;
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
            if (ret == 0)
            {
                LOG_INFO("connect sync ok");
                client->connected = true;
            }
            else if (errno == EINPROGRESS)
            {
                LOG_INFO("connect in progress, wait handshake...");
                if (check_connect_result(client->sockfd, 2000))
                {
                    LOG_INFO("tcp connect success!");
                    client->connected = true;
                }
                else
                {
                    close(client->sockfd);
                    client->sockfd = -1;
                    usleep(client->cfg.reconnect_ms * 1000);
                    continue;
                }
            }
            else
            {
                LOG_WARN("connect immediate fail errno:%d", errno);
                close(client->sockfd);
                client->sockfd = -1;
                usleep(client->cfg.reconnect_ms * 1000);
                continue;
            }
            client->last_heartbeat_ms = time_utils_get_ms();
            client->last_recv_ms = time_utils_get_ms();
        }

        uint64_t now_ms = time_utils_get_ms();

        // ========== 链路超时检测（新增） ==========
        if (now_ms - client->last_recv_ms >= client->cfg.link_timeout_ms)
        {
            LOG_WARN("tcp link timeout, no data received, close");
            client->connected = false;
            continue;
        }

        // ========== 心跳发送 ==========
        if (now_ms - client->last_heartbeat_ms >= client->cfg.heartbeat_ms)
        {
            uint8_t heartbeat_pkg[] = {0xAA};
            tcp_client_send(client, heartbeat_pkg, sizeof(heartbeat_pkg));
            client->last_heartbeat_ms = now_ms;
            LOG_INFO("send heartbeat pkg");
        }

        // ========== 接收逻辑 ==========
        ret = read(client->sockfd, tmp_buf, sizeof(tmp_buf));
        if (ret > 0)
        {
            block_ringbuf_write_batch(client->recv_rb, tmp_buf, ret);
            client->last_recv_ms = now_ms; // 收到数据，刷新时间戳
        }
        else if (ret == 0)
        {
            LOG_WARN("server closed connection");
            client->connected = false;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 无数据，不报错
            }
            else
            {
                LOG_ERROR("sock fatal error, errno=%d", errno);
                client->connected = false;
            }
        }

        // ========== ✅ 新增：发送缓冲区轮询写出 ==========
        if (client->connected)
        {
            while(1)
            {
                // 从发送ringbuf读出一包
                int rlen = block_ringbuf_read(&client->send_rb, send_buf, sizeof(send_buf));
                if (rlen <= 0)
                {
                    break; // 发送缓冲空了，退出发送循环
                }
                ssize_t wlen = write(client->sockfd, send_buf, rlen);
                if (wlen < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // 内核发送缓冲区满，把数据放回ringbuf，下次循环再发
                        block_ringbuf_write_head(&client->send_rb, send_buf, rlen);
                        break;
                    }
                    LOG_ERROR("write fatal error errno=%d", errno);
                    client->connected = false;
                    break;
                }
                // write成功，继续读下一段数据发送
            }
        }

        usleep(1000);
    }
    LOG_INFO("tcp client work thread exit");
    return NULL;
}

// ✅ 重写 send：只写入发送ringbuf，不再直接write
int tcp_client_send(tcp_client_t *client, const uint8_t *buf, uint32_t len)
{
    if(client == NULL || !client->connected || client->sockfd < 0)
    {
        LOG_WARN("tcp not connected, send drop");
        return -1;
    }
    int ret = block_ringbuf_write(&client->send_rb, buf, len);
    if(ret <= 0)
    {
        LOG_WARN("tcp send_rb full, packet drop");
        return -1;
    }
    return ret;
}

int tcp_client_start(tcp_client_t *client)
{
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
