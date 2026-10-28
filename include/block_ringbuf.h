#ifndef BLOCK_RINGBUF_H
#define BLOCK_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include "mutex_cond.h"

/**
 * @brief 阻塞型环形缓冲区
 * 基于mutex+cond实现，空读阻塞 / 满写阻塞
 */
typedef struct {
    uint8_t        *buf;         // 缓冲区内存
    uint32_t       size;         // 缓冲区总大小
    uint32_t       read_idx;      // 读下标
    uint32_t       write_idx;     // 写下标
    mutex_cond_t   mc;            // 锁+条件变量
    bool           init_ok;
    bool           stop_request;   //  新增：全局停止标记
} block_ringbuf_t;

/**
 * @brief 初始化阻塞环形缓冲区
 * @param rb 对象指针
 * @param buf 用户提供的内存
 * @param size 内存字节大小
 * @return 0成功
 */
int block_ringbuf_init(block_ringbuf_t *rb, uint8_t *buf, uint32_t size);

/**
 * @brief 阻塞批量写入
 * @param rb 环形缓冲句柄
 * @param buf 数据源
 * @param len 想要写入多少字节
 * @return 成功返回实际写入字节数（这里我们实现：凑够空间才返回，保证写入全部len字节）
 */
int block_ringbuf_write_batch(block_ringbuf_t *rb, const uint8_t *buf, uint32_t len);

/**
 * @brief 阻塞批量读取
 * @param rb 环形缓冲句柄
 * @param buf 接收缓冲区
 * @param len 想要读取多少字节
 * @return 成功返回读到的字节数，会阻塞直到拿到全部len字节
 */
int block_ringbuf_read_batch(block_ringbuf_t *rb, uint8_t *buf, uint32_t len);

/**
 * @brief 销毁环形缓冲
 */
void block_ringbuf_destroy(block_ringbuf_t *rb);

/**
 * @brief 发送停止请求，唤醒所有阻塞等待的线程
 */
void block_ringbuf_wakeup(block_ringbuf_t *rb);

#endif