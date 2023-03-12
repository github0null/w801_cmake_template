#undef LOG_TAG
#define LOG_TAG "main"

#include "main.h"

void UserMain(void)
{
    log_i("------------------------------------");
    log_i("app launched !");

    tls_sys_clk_set(CPU_CLK_240M);
    tls_sys_clk sys_clk;
    tls_sys_clk_get(&sys_clk);

    log_i(" - cpu  freq: %d MHz", sys_clk.cpuclk);
    log_i(" - apb  freq: %d MHz", sys_clk.apbclk);
    log_i(" - wlan freq: %d MHz", sys_clk.wlanclk);

    log_i("------------------------------------");

    while (true) {
        log_d("hello world !");
        tls_os_time_delay_ms(1000);
    }
}
