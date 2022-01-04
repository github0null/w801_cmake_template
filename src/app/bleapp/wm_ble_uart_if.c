/*****************************************************************************
**
**  Name:           wm_uart_ble_if.c
**
**  Description:    This file contains the  implemention of uart_ble_passthrough
**
*****************************************************************************/
#include <assert.h>

#include "wm_bt_config.h"

#if (WM_NIMBLE_INCLUDED == CFG_ON)

#include "host/ble_hs.h"
#include "wm_bt_app.h"
#include "wm_ble_uart_if.h"
#include "wm_ble_server_api_demo.h"
#include "wm_ble_client_api_demo.h"
#include "wm_bt_util.h"
#include "wm_mem.h"




static ringbuffer_t *g_rb_ptr = NULL;
static uint8_t g_uart_id = -1;
static tls_ble_uart_mode_t g_bum = BLE_UART_SERVER_MODE;

static struct ble_npl_callout g_async_send_timer;

#define RING_BUFFER_SIZE (4096)


static void wm_uart_async_write(uint8_t *p_data, uint16_t length)
{
    //TLS_BT_APPL_TRACE_API("%s , send to uart %d bytes\r\n", __FUNCTION__, length);
    tls_uart_write_async(g_uart_id, p_data, length);    
}

static void wm_uart_async_send_cb(struct ble_npl_event *evt)
{
    uint32_t cache_length = 0;
    uint32_t gatt_mtu = 0;
    uint32_t send_len = 0;
    uint8_t send_buffer[256];
    int rc;
    
    cache_length = bt_ringbuffer_size(g_rb_ptr);
    gatt_mtu = tls_ble_server_demo_api_get_mtu();

    send_len = MIN(cache_length, gatt_mtu);
    
    bt_ringbuffer_peek(g_rb_ptr,0,send_buffer,send_len);
    
    if(g_bum == BLE_UART_SERVER_MODE)
    {
        rc = tls_ble_server_demo_api_send_msg(send_buffer, send_len);
    }else
    {
        rc = tls_ble_client_demo_api_send_msg(send_buffer, send_len);
    } 
    
    if(rc == 0)
    {
        bt_ringbuffer_delete(g_rb_ptr,send_len);
    }else
    {
        //TLS_BT_APPL_TRACE_ERROR("Send to remote via ble failed, check reason=%d\r\n", bt_status);
        ble_npl_callout_reset(&g_async_send_timer,ble_npl_time_ms_to_ticks32(10));
    } 
}
static void wm_uart_async_read_cb(int size, void *user_data)
{
    int read_out = 0;
    uint32_t cache_length = 0;

    if(size <= 0) return;

    if(bt_ringbuffer_available(g_rb_ptr)< size)
    {
        TLS_BT_APPL_TRACE_WARNING("ble uart cache buffer is full, mode[%d]\r\n", g_bum);
        return;
    }
    
    uint8_t *tmp_ptr = tls_mem_alloc(size);
    
    cache_length = bt_ringbuffer_size(g_rb_ptr);
    
    read_out = tls_uart_read(g_uart_id, tmp_ptr, size);

    bt_ringbuffer_insert(g_rb_ptr, tmp_ptr, read_out);  

    /*if sending stopped or not started, triger to send immediatelly*/
    if(cache_length == 0)
    {
        ble_npl_callout_reset(&g_async_send_timer,ble_npl_time_ms_to_ticks32(5));
    }

    tls_mem_free(tmp_ptr);
}
/*
 * EXPORTED FUNCTION DEFINITIONS
 ****************************************************************************************
 */

int tls_ble_uart_init(tls_ble_uart_mode_t mode, uint8_t uart_id, tls_uart_options_t *p_hci_if)
{
    int rc;
    
    TLS_BT_APPL_TRACE_API("%s , uart_id=%d\r\n", __FUNCTION__, uart_id);

    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }    
    
    if(g_rb_ptr) return BLE_HS_EALREADY;

    if(mode == BLE_UART_SERVER_MODE)
    {
        rc = tls_ble_server_demo_api_init(wm_uart_async_write);
    }else if(mode == BLE_UART_CLIENT_MODE)
    {
        rc = tls_ble_client_demo_api_init(wm_uart_async_write);
    }else
    {
        return BLE_HS_EINVAL;
    }
    
    if(rc != 0)
    {
        return rc;
    }
    
    g_rb_ptr = bt_ringbuffer_init(RING_BUFFER_SIZE);
    if(g_rb_ptr == NULL)
    {
        
        if(mode == BLE_UART_SERVER_MODE)
        {
            tls_ble_server_demo_api_deinit();
        }else if(mode == BLE_UART_CLIENT_MODE)
        {
            tls_ble_client_demo_api_deinit();
        }
        return BLE_HS_ENOMEM;
    }

    rc = tls_uart_port_init(uart_id, NULL, 0);
    if(rc != WM_SUCCESS)
    {
        if(mode == BLE_UART_SERVER_MODE)
        {
            tls_ble_server_demo_api_deinit();
        }else if(mode == BLE_UART_CLIENT_MODE)
        {
            tls_ble_client_demo_api_deinit();
        }

        bt_ringbuffer_free(g_rb_ptr);
        g_rb_ptr = NULL;
        return BLE_HS_EAPP;
    }
    
    g_uart_id = uart_id;
    g_bum = mode;

    ble_npl_callout_init(&g_async_send_timer, nimble_port_get_dflt_eventq(), wm_uart_async_send_cb, NULL);
    tls_uart_rx_callback_register(uart_id, wm_uart_async_read_cb, (void *)NULL);

    return 0;
}

int tls_ble_uart_deinit(tls_ble_uart_mode_t mode,uint8_t uart_id)
{
    int rc = 0;

    //TODO deinit uart interface???

    if(mode == BLE_UART_SERVER_MODE)
    {
        rc = tls_ble_server_demo_api_deinit();
    }else if(mode == BLE_UART_CLIENT_MODE)
    {
        rc = tls_ble_client_demo_api_deinit();
    }

    if(rc != 0) return BLE_HS_EAPP;
    
    if(ble_npl_callout_is_active(&g_async_send_timer)) ble_npl_callout_stop(&g_async_send_timer);
    ble_npl_callout_deinit(&g_async_send_timer);

    /*clear the async recv function*/
    tls_uart_rx_callback_register(uart_id, NULL, (void *)NULL);

    
    if(g_rb_ptr)
    {
       bt_ringbuffer_free(g_rb_ptr); 
       g_rb_ptr = NULL;
    }

    return 0;
}
uint32_t tls_ble_uart_buffer_size()
{
    return bt_ringbuffer_size(g_rb_ptr);
}
uint32_t tls_ble_uart_buffer_available()
{
    return bt_ringbuffer_available(g_rb_ptr);
}

uint32_t tls_ble_uart_buffer_read(uint8_t *ptr, uint32_t length)
{
    return bt_ringbuffer_pop(g_rb_ptr, ptr, length);
}
uint32_t tls_ble_uart_buffer_peek(uint8_t *ptr, uint32_t length)
{
    return bt_ringbuffer_peek(g_rb_ptr,0,ptr,length);
}
uint32_t tls_ble_uart_buffer_delete(uint32_t length)
{
    return bt_ringbuffer_delete(g_rb_ptr,length);
}

#endif
