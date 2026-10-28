#include "time_utils.h"
#include "log_module.h"

int main(void)
{
    log_module_init(256,LOG_LEVEL_INFO);
    LOG_INFO("==== test time utils start ====");

    uint64_t t1 = time_utils_get_ms();
    LOG_INFO("t1 = %lu ms", t1);

    time_utils_sleep_ms(1200);

    uint64_t t2 = time_utils_get_ms();
    LOG_INFO("t2 = %lu ms", t2);
    LOG_INFO("diff = %lu ms", t2 - t1);

    uint64_t t_start = time_utils_get_ms();
    time_utils_sleep_ms(600);
    bool ret = time_utils_is_timeout(t_start, 500);
    LOG_INFO("is_timeout(500ms) = %d", ret);

    log_deinit();
    LOG_INFO("==== test time utils end ====");
    return 0;
}