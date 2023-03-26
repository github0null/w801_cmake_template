#undef LOG_TAG
#define LOG_TAG "main"

#include "main.h"

void UserMain(void *task_handle_ptr)
{
    log_i("------------------------------------");
    log_i("app launched !");

    tls_sys_clk_set(CPU_CLK_240M);
    tls_sys_clk sys_clk;
    tls_sys_clk_get(&sys_clk);

    log_i(" - sdk version: v%s", w800sdk_get_version_string());
    log_i(" - cpu    freq: %d MHz", sys_clk.cpuclk);
    log_i(" - apb    freq: %d MHz", sys_clk.apbclk);
    log_i(" - wlan   freq: %d MHz", sys_clk.wlanclk);

    u8 mac[6];

    if (tls_get_bt_mac_addr(mac) == TLS_EFUSE_STATUS_OK) {
        log_i(" - ble     mac: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    if (tls_get_mac_addr(mac) == TLS_EFUSE_STATUS_OK) {
        log_i(" - wifi    mac: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    log_i("------------------------------------");

    // 获取固件签名
    struct w800_img_signature_info_t sign_inf;
    if (w800sdk_get_img_signature(&sign_inf) == 0) {

        if (sign_inf.verify_passed) {
            log_i("firmware signature(%08x) is %s !", sign_inf.sign_addr, "verified");
        } else {
            log_e("firmware signature(%08x) is %s !", sign_inf.sign_addr, "invalid");
        }

        log_i(" - sha1:");
        log_i_hexdump("sha1", 20, sign_inf.image_sha1, 20);
        log_i(" - digital signature:");
        log_i_hexdump("sign", 16, sign_inf.sign_data, 128);
    }

    while (true) {
        log_d("hello world !");
        tls_os_time_delay_ms(1000);
    }
}
