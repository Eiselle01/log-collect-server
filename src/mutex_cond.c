#include "mutex_cond.h"
#include "log_module.h"
#include <pthread.h>
#include <string.h>

int mutex_cond_init(mutex_cond_t *mc)
{
    if (mc == NULL)
    {
        LOG_ERROR("mutex_cond_init: null pointer");
        return -1;
    }
    memset(mc, 0, sizeof(mutex_cond_t));

    int ret = pthread_mutex_init(&mc->mutex, NULL);
    if (ret != 0)
    {
        LOG_ERROR("pthread_mutex_init failed, ret=%d", ret);
        return -2;
    }

    ret = pthread_cond_init(&mc->cond, NULL);
    if (ret != 0)
    {
        LOG_ERROR("pthread_cond_init failed, ret=%d", ret);
        pthread_mutex_destroy(&mc->mutex);
        return -3;
    }

    mc->init_ok = true;
    LOG_INFO("mutex cond init success");
    return 0;
}

int mutex_cond_lock(mutex_cond_t *mc)
{
    if (mc == NULL || !mc->init_ok)
    {
        LOG_ERROR("mutex_cond_lock: uninitialized");
        return -1;
    }
    int ret = pthread_mutex_lock(&mc->mutex);
    if (ret != 0)
    {
        LOG_ERROR("pthread_mutex_lock failed, ret=%d", ret);
        return -2;
    }
    return 0;
}

int mutex_cond_unlock(mutex_cond_t *mc)
{
    if (mc == NULL || !mc->init_ok)
    {
        LOG_ERROR("mutex_cond_unlock: uninitialized");
        return -1;
    }
    int ret = pthread_mutex_unlock(&mc->mutex);
    if (ret != 0)
    {
        LOG_ERROR("pthread_mutex_unlock failed, ret=%d", ret);
        return -2;
    }
    return 0;
}

int mutex_cond_wait(mutex_cond_t *mc)
{
    if (mc == NULL || !mc->init_ok)
    {
        LOG_ERROR("mutex_cond_wait: uninitialized");
        return -1;
    }
    int ret = pthread_cond_wait(&mc->cond, &mc->mutex);
    if (ret != 0)
    {
        LOG_ERROR("pthread_cond_wait failed, ret=%d", ret);
        return -2;
    }
    return 0;
}

int mutex_cond_signal(mutex_cond_t *mc)
{
    if (mc == NULL || !mc->init_ok)
    {
        LOG_ERROR("mutex_cond_signal: uninitialized");
        return -1;
    }
    int ret = pthread_cond_signal(&mc->cond);
    if (ret != 0)
    {
        LOG_ERROR("pthread_cond_signal failed, ret=%d", ret);
        return -2;
    }
    return 0;
}

int mutex_cond_broadcast(mutex_cond_t *mc)
{
    if (mc == NULL || !mc->init_ok)
    {
        LOG_ERROR("mutex_cond_broadcast: uninitialized");
        return -1;
    }
    // 先调用函数，定义ret
    int ret = pthread_cond_broadcast(&mc->cond);
    if (ret != 0)
    {
        LOG_ERROR("pthread_cond_broadcast failed, ret=%d", ret);
        return -2;
    }
    return 0;
}

void mutex_cond_destroy(mutex_cond_t *mc)
{
    if (mc == NULL || !mc->init_ok)
    {
        return;
    }
    pthread_cond_destroy(&mc->cond);
    pthread_mutex_destroy(&mc->mutex);
    mc->init_ok = false;
    LOG_INFO("mutex cond destroy ok");
}