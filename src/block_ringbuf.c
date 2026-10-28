#include "block_ringbuf.h"
#include "log_module.h"
#include <string.h>

static inline uint32_t ringbuf_get_data_len(block_ringbuf_t *rb)
{
    if(rb->write_idx >= rb->read_idx)
    {
        return rb->write_idx - rb->read_idx;
    }
    else
    {
        return rb->size - rb->read_idx + rb->write_idx;
    }
}

static inline uint32_t ringbuf_get_free_len(block_ringbuf_t *rb)
{
    // 牺牲一格用来区分满/空，可用空间 = size -1 - 已存数据长度
    return (rb->size - 1) - ringbuf_get_data_len(rb);
}

int block_ringbuf_init(block_ringbuf_t *rb, uint8_t *buf, uint32_t size)
{
    if (rb == NULL || buf == NULL || size <= 1)
    {
        LOG_ERROR("block_ringbuf_init invalid param, size must >1");
        return -1;
    }
    memset(rb, 0, sizeof(block_ringbuf_t));

    rb->buf = buf;
    rb->size = size;
    rb->read_idx  = 0;
    rb->write_idx = 0;
    rb->stop_request = false;

    int ret = mutex_cond_init(&rb->mc);
    if (ret != 0)
    {
        LOG_ERROR("mutex cond init fail in block ringbuf");
        return -2;
    }

    rb->init_ok = true;
    LOG_INFO("block ringbuf init success, size=%u", size);
    return 0;
}

int block_ringbuf_write_batch(block_ringbuf_t *rb, const uint8_t *buf, uint32_t len)
{
    if (rb == NULL || !rb->init_ok || buf == NULL || len == 0)
    {
        LOG_ERROR("block_ringbuf_write_batch invalid param");
        return -1;
    }

    uint32_t write_remain = len;
    const uint8_t *p_src  = buf;

    while (write_remain > 0)
    {
        mutex_cond_lock(&rb->mc);

        // ✅ 同时判断：缓冲满 并且 没有收到停止请求才继续等待
        while (ringbuf_get_free_len(rb) == 0 && !rb->stop_request)
        {
            LOG_INFO("block ringbuf full, write waiting for free space");
            mutex_cond_wait(&rb->mc);
        }

        // 如果收到停止请求直接退出，不再继续写
        if(rb->stop_request)
        {
            mutex_cond_unlock(&rb->mc);
            LOG_WARN("block ringbuf receive stop request, abort write");
            return -2;
        }

        uint32_t free_len = ringbuf_get_free_len(rb);
        uint32_t w_len = (write_remain < free_len) ? write_remain : free_len;

        uint32_t tail_space = rb->size - rb->write_idx;
        if (w_len <= tail_space)
        {
            // 不需要跨边界，一次性写入
            memcpy(&rb->buf[rb->write_idx], p_src, w_len);
            rb->write_idx += w_len;
        }
        else
        {
            // 跨边界：先写到buffer末尾，剩下从头开始
            memcpy(&rb->buf[rb->write_idx], p_src, tail_space);
            uint32_t left = w_len - tail_space;
            memcpy(&rb->buf[0], p_src + tail_space, left);
            rb->write_idx = left;
        }

        write_remain -= w_len;
        p_src += w_len;

        mutex_cond_signal(&rb->mc);
        mutex_cond_unlock(&rb->mc);
    }
    return len;
}

int block_ringbuf_read_batch(block_ringbuf_t *rb, uint8_t *buf, uint32_t len)
{
    if (rb == NULL || !rb->init_ok || buf == NULL || len == 0)
    {
        LOG_ERROR("block_ringbuf_read_batch invalid param");
        return -1;
    }

    uint32_t read_remain = len;
    uint8_t *p_dst = buf;

    while (read_remain > 0)
    {
        mutex_cond_lock(&rb->mc);

        // ✅ 缓冲区为空并且没有停止请求才阻塞等待
        while (ringbuf_get_data_len(rb) == 0 && !rb->stop_request)
        {
            LOG_INFO("block ringbuf empty, read waiting for data");
            mutex_cond_wait(&rb->mc);
        }

        // 如果收到停止请求直接退出
        if(rb->stop_request)
        {
            mutex_cond_unlock(&rb->mc);
            LOG_WARN("block ringbuf receive stop request, abort read");
            return -2;
        }

        uint32_t data_len = ringbuf_get_data_len(rb);
        uint32_t r_len = (read_remain < data_len) ? read_remain : data_len;

        uint32_t tail_data = rb->size - rb->read_idx;
        if (r_len <= tail_data)
        {
            memcpy(p_dst, &rb->buf[rb->read_idx], r_len);
            rb->read_idx += r_len;
        }
        else
        {
            memcpy(p_dst, &rb->read_idx, tail_data);
            uint32_t left = r_len - tail_data;
            memcpy(p_dst + tail_data, &rb->buf[0], left);
            rb->read_idx = left;
        }

        read_remain -= r_len;
        p_dst += r_len;

        mutex_cond_signal(&rb->mc);
        mutex_cond_unlock(&rb->mc);
    }
    return len;
}

void block_ringbuf_wakeup(block_ringbuf_t *rb)
{
    if(rb == NULL || !rb->init_ok)
    {
        return;
    }
    mutex_cond_lock(&rb->mc);
    rb->stop_request = true;
    // broadcast 一次性唤醒所有卡在wait里的读写线程
    mutex_cond_broadcast(&rb->mc);
    mutex_cond_unlock(&rb->mc);
    LOG_INFO("block ringbuf wakeup, set stop flag and broadcast");
}

void block_ringbuf_destroy(block_ringbuf_t *rb)
{
    if (rb == NULL || !rb->init_ok)
    {
        return;
    }
    mutex_cond_destroy(&rb->mc);
    rb->buf = NULL;
    rb->init_ok = false;
    LOG_INFO("block ringbuf destroy ok");
}