#ifndef __WM_BLE_SERVER_DEMO_H__
#define __WM_BLE_SERVER_DEMO_H__
#include "wm_bt.h"

int tls_ble_server_demo_api_init(tls_ble_output_func_ptr output_func_ptr);
int tls_ble_server_demo_api_deinit();
uint32_t tls_ble_server_demo_api_get_mtu();
int tls_ble_server_demo_api_send_msg(uint8_t *data, int data_len);


#endif

