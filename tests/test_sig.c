#include <stdio.h>
#include "sig_handle.h"
#include <unistd.h>

int main(void)
{
    printf("sig handle test, press Ctrl+C to exit\n");
    sig_handle_init();

    while (!sig_handle_need_exit())
    {
        printf("running...\n");
        sleep(1);
    }

    sig_handle_deinit();
    printf("program exit\n");
    return 0;
}