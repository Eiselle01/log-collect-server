#ifndef LOG_MODULE_H
#define LOG_MODULE_H

#include <stdint.h>
#include <stdbool.h>

// 日志等级
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

/**
 * @brief 日志模块初始化
 * @param buf_size 内部环形缓冲区大小
 * @param level 全局日志过滤等级，低于该等级不输出
 * @return 0成功，-1失败
 */
int log_module_init(uint32_t buf_size, log_level_t level);

/**
 * @brief 反初始化，刷完剩余日志，释放资源
 */
void log_module_deinit(void);

/**
 * @brief 修改全局日志过滤等级
 */
void log_module_set_level(log_level_t level);

// 底层打印接口，推荐用下面宏
void log_print(log_level_t level, const char *file, int line, const char *fmt, ...);

// 方便使用的宏
#define LOG_DEBUG(fmt, ...) log_print(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_print(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_print(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
/*异步日志思路：业务线程调用 LOG_INFO 只是把日志字符串丢进 ringbuf，立刻返回，不阻塞；单独一个日志刷盘线程负责从 ringbuf 取出、格式化输出。
面试高频：同步日志阻塞问题、异步日志丢日志风险、FILE LINE 宏。*/