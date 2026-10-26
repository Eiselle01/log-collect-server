#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "block_ringbuf.h"

// TCP客户端配置参数
typedef struct {
    char        server_ip[32];
    uint16_t    server_port;
    uint32_t    reconnect_ms;   // 重连间隔 ms
    uint32_t    heartbeat_ms;   // 心跳发送周期 ms
} tcp_client_cfg_t;

typedef struct {
    tcp_client_cfg_t cfg;
    int sockfd;
    bool connected;
    bool stop_request;
    block_ringbuf_t *recv_rb;    // 接收的数据写入这个阻塞环形缓冲
} tcp_client_t;

/**
 * @brief 初始化tcp client，绑定接收环形缓冲
 */
int tcp_client_init(tcp_client_t *client, const tcp_client_cfg_t *cfg, block_ringbuf_t *recv_rb);

/**
 * @brief 启动后台连接&接收线程
 */
int tcp_client_start(tcp_client_t *client);

/**
 * @brief 发送数据到服务端
 */
int tcp_client_send(tcp_client_t *client, const uint8_t *buf, uint32_t len);

/**
 * @brief 停止tcp客户端，关闭socket、退出线程
 */
void tcp_client_stop(tcp_client_t *client);

/**
 * @brief 销毁资源
 */
void tcp_client_destroy(tcp_client_t *client);

#endif