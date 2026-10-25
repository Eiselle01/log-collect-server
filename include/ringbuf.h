#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t write_pos;
    uint32_t read_pos;
    pthread_mutex_t lock;
    pthread_cond_t cond_not_full;    // 不满条件：生产者等待
    pthread_cond_t cond_not_empty;   // 非空条件：消费者等待
} ringbuf_t;

ringbuf_t *ringbuf_create(uint32_t buf_size);
void ringbuf_destroy(ringbuf_t *rb);

/**
 * @brief 阻塞写：缓冲区满就阻塞等待，直到有空位，不会丢数据
 * @return 写入字节
 */
uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len);

/**
 * @brief 阻塞读：缓冲区空就阻塞等待，直到读到数据
 */
uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len);

uint32_t ringbuf_get_used(ringbuf_t *rb);
uint32_t ringbuf_get_free(ringbuf_t *rb);
void ringbuf_clear(ringbuf_t *rb);

#endif