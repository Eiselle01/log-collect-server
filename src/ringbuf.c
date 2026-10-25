#include "ringbuf.h"
#include <stdlib.h>
#include <string.h>

static inline uint32_t ringbuf_calc_used(ringbuf_t *rb)
{
    uint32_t used;
    if (rb->write_pos >= rb->read_pos)
    {
        used = rb->write_pos - rb->read_pos;
    }
    else
    {
        used = rb->size - (rb->read_pos - rb->write_pos);
    }
    return used;
}

ringbuf_t *ringbuf_create(uint32_t buf_size)
{
    if (buf_size == 0)
        return NULL;

    ringbuf_t *rb = (ringbuf_t *)malloc(sizeof(ringbuf_t));
    if (rb == NULL)
        return NULL;

    rb->buf = (uint8_t *)malloc(buf_size);
    if (rb->buf == NULL)
    {
        free(rb);
        return NULL;
    }

    rb->size = buf_size;
    rb->write_pos = 0;
    rb->read_pos = 0;

    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->cond_not_full, NULL);
    pthread_cond_init(&rb->cond_not_empty, NULL);
    return rb;
}

void ringbuf_destroy(ringbuf_t *rb)
{
    if (rb == NULL)
        return;

    pthread_mutex_lock(&rb->lock);
    if (rb->buf != NULL)
    {
        free(rb->buf);
        rb->buf = NULL;
    }
    rb->size = 0;
    rb->write_pos = 0;
    rb->read_pos = 0;
    pthread_mutex_unlock(&rb->lock);

    pthread_cond_destroy(&rb->cond_not_full);
    pthread_cond_destroy(&rb->cond_not_empty);
    pthread_mutex_destroy(&rb->lock);
    free(rb);
}

uint32_t ringbuf_get_used(ringbuf_t *rb)
{
    if (rb == NULL)
        return 0;
    pthread_mutex_lock(&rb->lock);
    uint32_t used = ringbuf_calc_used(rb);
    pthread_mutex_unlock(&rb->lock);
    return used;
}

uint32_t ringbuf_get_free(ringbuf_t *rb)
{
    if (rb == NULL)
        return 0;
    pthread_mutex_lock(&rb->lock);
    // 依旧保留预留1字节区分空满
    uint32_t free = (rb->size - 1) - ringbuf_calc_used(rb);
    pthread_mutex_unlock(&rb->lock);
    return free;
}

uint32_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    if (rb == NULL || data == NULL || len == 0)
        return 0;

    pthread_mutex_lock(&rb->lock);

    // ===== 重点：while等待，防止虚假唤醒 =====
    // 满条件：used == size -1
    while (ringbuf_calc_used(rb) == (rb->size - 1))
    {
        // 自动释放lock，休眠；被唤醒后重新拿到lock再返回
        pthread_cond_wait(&rb->cond_not_full, &rb->lock);
    }

    uint32_t free_space = (rb->size - 1) - ringbuf_calc_used(rb);
    uint32_t w_len = (len > free_space) ? free_space : len;

    for (uint32_t i = 0; i < w_len; i++)
    {
        rb->buf[rb->write_pos] = data[i];
        rb->write_pos = (rb->write_pos + 1) % rb->size;
    }

    // ✅ 写完数据，缓冲区不再是空，唤醒等待数据的消费者
    pthread_cond_signal(&rb->cond_not_empty);

    pthread_mutex_unlock(&rb->lock);
    return w_len;
}

uint32_t ringbuf_read(ringbuf_t *rb, uint8_t *data, uint32_t len)
{
    if (rb == NULL || data == NULL || len == 0)
        return 0;

    pthread_mutex_lock(&rb->lock);

    // ===== while等待：缓冲区为空就等数据 =====
    while (ringbuf_calc_used(rb) == 0)
    {
        pthread_cond_wait(&rb->cond_not_empty, &rb->lock);
    }

    uint32_t used = ringbuf_calc_used(rb);
    uint32_t r_len = (len > used) ? used : len;

    for (uint32_t i = 0; i < r_len; i++)
    {
        data[i] = rb->buf[rb->read_pos];
        rb->read_pos = (rb->read_pos + 1) % rb->size;
    }

    // ✅ 读完，缓冲区不再满，唤醒等待空位的生产者
    pthread_cond_signal(&rb->cond_not_full);

    pthread_mutex_unlock(&rb->lock);
    return r_len;
}

void ringbuf_clear(ringbuf_t *rb)
{
    if (rb == NULL)
        return;
    pthread_mutex_lock(&rb->lock);
    rb->read_pos = 0;
    rb->write_pos = 0;
    // 清空后缓冲有空位，唤醒生产者
    pthread_cond_signal(&rb->cond_not_full);
    pthread_mutex_unlock(&rb->lock);
}