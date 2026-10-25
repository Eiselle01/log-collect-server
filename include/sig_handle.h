#ifndef SIG_HANDLE_H
#define SIG_HANDLE_H

#include <stdbool.h>
#include <signal.h>

/**
 * @brief 初始化信号处理，注册SIGINT、SIGTERM
 * 调用之后，Ctrl+C / kill 会触发优雅退出标记
 * @return 0成功，-1失败
 */
int sig_handle_init(void);

/**
 * @brief 查询是否收到退出信号（线程安全）
 * @return true 需要退出，false 继续运行
 */
bool sig_handle_need_exit(void);

/**
 * @brief 反初始化信号模块（可选，清理）
 */
void sig_handle_deinit(void);

#endif
