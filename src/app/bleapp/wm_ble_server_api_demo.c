#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "wm_bt_config.h"

#if (WM_NIMBLE_INCLUDED == CFG_ON)

#include "wm_bt_app.h"
#include "wm_bt_util.h"
#include "host/ble_hs.h"
#include "wm_ble_gap.h"
#include "wm_ble_uart_if.h"
#include "wm_ble_server_api_demo.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

typedef enum{
    BLE_SERVER_MODE_IDLE     = 0x00,
    BLE_SERVER_MODE_ADVERTISING = 0x01,
    BLE_SERVER_MODE_CONNECTED,
    BLE_SERVER_MODE_INDICATING,
    BLE_SERVER_MODE_EXITING
} ble_server_state_t;

static uint8_t g_ble_demo_prof_connected = 0;
static uint8_t g_ble_demo_indicate_enable = 0;
static tls_ble_output_func_ptr g_ble_uart_output_fptr = NULL;
static int g_mtu = 20;
static uint8_t g_ind_data[255];
static volatile uint8_t g_send_pending = 0;
static volatile ble_server_state_t g_ble_server_state = BLE_SERVER_MODE_IDLE;


/* ble attr write/notify handle */
uint16_t g_ble_demo_attr_indicate_handle;
uint16_t g_ble_demo_attr_write_handle;
uint16_t g_ble_demo_conn_handle ;



#define WM_GATT_SVC_UUID      0xFFF0
#define WM_GATT_INDICATE_UUID 0xFFF1
#define WM_GATT_WRITE_UUID    0xFFF2


static int
gatt_svr_chr_demo_access_func(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);


/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

static int
gatt_svr_chr_demo_write_func(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);


static const struct ble_gatt_svc_def gatt_demo_svr_svcs[] = {
    {
        /* Service: uart */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(WM_GATT_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
                .uuid = BLE_UUID16_DECLARE(WM_GATT_WRITE_UUID),
                .val_handle = &g_ble_demo_attr_write_handle,
                .access_cb = gatt_svr_chr_demo_access_func,
                .flags = BLE_GATT_CHR_F_WRITE,
            },{
                .uuid = BLE_UUID16_DECLARE(WM_GATT_INDICATE_UUID),
                .val_handle = &g_ble_demo_attr_indicate_handle,
                .access_cb = gatt_svr_chr_demo_access_func,
                .flags = BLE_GATT_CHR_F_INDICATE,
            },{
              0, /* No more characteristics in this service */
            } 
         },
    },

    {
        0, /* No more services */
    },
};

int wm_ble_server_api_demo_adv(bool enable)
{
    int rc;
    
    if(enable)
    {
        struct ble_hs_adv_fields fields;
        const char *name;
        
        /**
         *  Set the advertisement data included in our advertisements:
         *     o Flags (indicates advertisement type and other general info).
         *     o Device name.
         *     o user specific field (winner micro).
         */
        
        memset(&fields, 0, sizeof fields);
        
        /* Advertise two flags:
         *     o Discoverability in forthcoming advertisement (general)
         *     o BLE-only (BR/EDR unsupported).
         */
        fields.flags = BLE_HS_ADV_F_DISC_GEN |
                       BLE_HS_ADV_F_BREDR_UNSUP;
        
        
        name = ble_svc_gap_device_name();
        fields.name = (uint8_t *)name;
        fields.name_len = strlen(name);
        fields.name_is_complete = 1;
        
        fields.uuids16 = (ble_uuid16_t[]){
            BLE_UUID16_INIT(0xFFF0)
        };
        fields.num_uuids16 = 1;
        fields.uuids16_is_complete = 1;

        
        rc = ble_gap_adv_set_fields(&fields);
        if (rc != 0) {
            TLS_BT_APPL_TRACE_ERROR("error setting advertisement data; rc=%d\r\n", rc);
            return rc;
        }
        
        /* As own address type we use hard-coded value, because we generate
              NRPA and by definition it's random */
        rc = tls_nimble_gap_adv(WM_BLE_ADV_IND, 0);
        assert(rc == 0);

    }else
    {
        rc = tls_nimble_gap_adv(WM_BLE_ADV_STOP, 0);
    }

    return rc;
}
/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

static int
gatt_svr_chr_demo_access_func(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int i = 0;
    struct os_mbuf *om = ctxt->om;
    
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
              while(om) {
                  if(g_ble_uart_output_fptr)
                  {
                    g_ble_uart_output_fptr((uint8_t *)om->om_data, om->om_len);
                    
                  }else
                  {
                     print_bytes(om->om_data, om->om_len); 
                  }
                  om = SLIST_NEXT(om, om_next);
              }
              return 0;
        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

int
wm_ble_server_demo_gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_demo_svr_svcs);
    if (rc != 0) {
        goto err;
    }

    rc = ble_gatts_add_svcs(gatt_demo_svr_svcs);
    if (rc != 0) {
        return rc;
    }

err:
    return rc;
}


static uint8_t ss = 0x00;

static void ble_server_indication_sent_cb(int conn_id, int status)
{
	int len = 0;
    tls_bt_status_t ret;

    g_send_pending = 0;
    if(!g_ble_demo_indicate_enable)
    {
        TLS_BT_APPL_TRACE_DEBUG("Indicate disabled... when trying to send...\r\n");
        return;
    }

    if(g_ble_uart_output_fptr == NULL)
    {
        memset(g_ind_data, ss, sizeof(g_ind_data));
        ss++;
        if(ss > 0xFE) ss = 0x00;
    	tls_ble_server_demo_api_send_msg(g_ind_data, g_mtu); 

    }else
    {
        len = tls_ble_uart_buffer_size();
        len = MIN(len, g_mtu);

        if(len)
        {
            tls_ble_uart_buffer_peek(g_ind_data, len);
            ret = tls_ble_server_demo_api_send_msg(g_ind_data, len); 
            if(ret == TLS_BT_STATUS_SUCCESS)
            {
                tls_ble_uart_buffer_delete(len);
                g_send_pending = 1;
            }else
            {
               TLS_BT_APPL_TRACE_DEBUG("server send via ble failed(%d), retry...\r\n", ret); 
            }
        }
    }
}

static void wm_ble_server_demo_start_indicate(void *arg)
{
    int len;
    int rc;
    uint8_t *tmp_ptr = NULL;
    tls_bt_status_t status;
    
    /*No uart ble interface*/
    if(g_ble_uart_output_fptr == NULL)
    {
	    rc = tls_ble_server_demo_api_send_msg(g_ind_data, g_mtu);
        TLS_BT_APPL_TRACE_DEBUG("Indicating sending...rc=%d\r\n", rc);
    }else
    {
        /*check and send*/
        len = tls_ble_uart_buffer_size();
        len = MIN(len, g_mtu);

        if(len)
        {
            tls_ble_uart_buffer_peek(g_ind_data, len);
            status = tls_ble_server_demo_api_send_msg(g_ind_data, g_mtu);
            if(status == TLS_BT_STATUS_SUCCESS)
            {
                tls_ble_uart_buffer_delete(len);
            }else
            {
               TLS_BT_APPL_TRACE_DEBUG("Server send failed(%d), retry...\r\n", status); 
               tls_bt_async_proc_func(wm_ble_server_demo_start_indicate,(void*)g_ble_demo_indicate_enable,1000);
            }
        }
    }    
}
static void conn_param_update_cb(uint16_t conn_handle, int status, void *arg)
{
	TLS_BT_APPL_TRACE_DEBUG("conn param update complete; conn_handle=%d status=%d\n",
		       conn_handle, status);
}

static void wm_ble_server_demo_conn_param_update_slave()
{
	int rc;
	struct ble_l2cap_sig_update_params params;

	params.itvl_min = 0x0006;
	params.itvl_max = 0x0006;
	params.slave_latency = 0;
	params.timeout_multiplier = 0x07d0;

	rc = ble_l2cap_sig_update(g_ble_demo_conn_handle, &params,
		conn_param_update_cb, NULL);
	assert(rc == 0);
}

static int ble_gap_evt_cb(struct ble_gap_event *event, void *arg)
{
    int rc;
    struct ble_gap_conn_desc desc;
    
    switch(event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            g_ble_demo_prof_connected = 1;
            g_send_pending = 0;
            
            TLS_BT_APPL_TRACE_DEBUG("connected status=%d handle=%d,g_ble_demo_attr_indicate_handle=%d\r\n",event->connect.status, g_ble_demo_conn_handle, g_ble_demo_attr_indicate_handle );
            if (event->connect.status == 0) {
                g_ble_server_state = BLE_SERVER_MODE_CONNECTED;
                       //re set this flag, to prevent stop adv, but connected evt reported when deinit this demo
                g_ble_demo_conn_handle = event->connect.conn_handle;
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                print_conn_desc(&desc);

#if MYNEWT_VAL(BLEPRPH_LE_PHY_SUPPORT)
                phy_conn_changed(event->connect.conn_handle);
#endif
            }
            TLS_BT_APPL_TRACE_DEBUG("\r\n");

            if (event->connect.status != 0) {
                /* Connection failed; resume advertising. */
                tls_nimble_gap_adv(WM_BLE_ADV_IND, 0);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            g_ble_demo_prof_connected = 0;
            g_ble_demo_indicate_enable = 0;
            g_send_pending = 0;
            TLS_BT_APPL_TRACE_DEBUG("disconnect reason=%d,state=%d\r\n", event->disconnect.reason,g_ble_server_state);
            if(g_ble_server_state == BLE_SERVER_MODE_EXITING)
            {
                if(g_ble_uart_output_fptr)
                {
                    g_ble_uart_output_fptr = NULL;
                }
                
                g_ble_server_state = BLE_SERVER_MODE_IDLE;
            }else
            {
                rc = tls_nimble_gap_adv(WM_BLE_ADV_IND, 0);
                if(!rc)
                {
                  g_ble_server_state = BLE_SERVER_MODE_ADVERTISING;  
                }
            }
            #if 0
            if(event->disconnect.reason == 534)
            {
                //hci error code:  0x16 + 0x200 = 534; //local host terminate the connection;
            }else
            {
                tls_nimble_gap_adv(WM_BLE_ADV_IND, 0);
            }
            #endif
            
            break;
        case BLE_GAP_EVENT_NOTIFY_TX:
            if(event->notify_tx.status == BLE_HS_EDONE)
            {
                ble_server_indication_sent_cb(event->notify_tx.attr_handle, event->notify_tx.status);
            }else
            {
                /*Application will handle other cases*/
            }
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            TLS_BT_APPL_TRACE_DEBUG("subscribe indicate(%d,%d)\r\n", event->subscribe.prev_indicate,event->subscribe.cur_indicate );
            g_ble_demo_indicate_enable = event->subscribe.cur_indicate;
            if(g_ble_demo_indicate_enable)
            {
                g_ble_server_state = BLE_SERVER_MODE_INDICATING;
                /*To reach the max passthrough,  in ble_uart mode, I conifg the min connection_interval*/
                if(g_ble_uart_output_fptr)
                {
                    tls_bt_async_proc_func(wm_ble_server_demo_conn_param_update_slave, NULL, 30);
                }
                tls_bt_async_proc_func(wm_ble_server_demo_start_indicate,(void*)g_ble_demo_indicate_enable, 30);
            }else
            {
                if(g_ble_server_state != BLE_SERVER_MODE_EXITING)
                {
                    g_ble_server_state = BLE_SERVER_MODE_CONNECTED;
                }
            }
            break;
        case BLE_GAP_EVENT_MTU:
            TLS_BT_APPL_TRACE_DEBUG("wm ble dm mtu changed to(%d)\r\n", event->mtu.value);
            /*nimBLE config prefered ATT_MTU is 256. here 256-12 = 244. */
            /* preamble(1)+access address(4)+pdu(2~257)+crc*/
            /* ATT_MTU(247):pdu= pdu_header(2)+l2cap_len(2)+l2cap_chn(2)+mic(4)*/
            /* GATT MTU(244): ATT_MTU +opcode+chn*/
            g_mtu = min(event->mtu.value - 12, 244);
            
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            /* We already have a bond with the peer, but it is attempting to
             * establish a new secure link.  This app sacrifices security for
             * convenience: just throw away the old bond and accept the new link.
             */
            /* Delete the old bond. */
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);
            
            TLS_BT_APPL_TRACE_DEBUG("!!!BLE_GAP_EVENT_REPEAT_PAIRING\r\n");
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            TLS_BT_APPL_TRACE_DEBUG(">>>BLE_GAP_EVENT_REPEAT_PAIRING\r\n");
            return 0;

            
        default:
            break;
    }

    return 0;
}

/*
 * EXPORTED FUNCTION DEFINITIONS
 ****************************************************************************************
 */

int tls_ble_server_demo_api_init(tls_ble_output_func_ptr output_func_ptr)
{
    int rc = BLE_HS_EAPP;

    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }
    
    TLS_BT_APPL_TRACE_DEBUG("%s, state=%d\r\n", __FUNCTION__, g_ble_server_state);
    
    if(g_ble_server_state == BLE_SERVER_MODE_IDLE)
    {
        g_ble_demo_prof_connected = 0;
        
        //step 0: reset other services. Note 
        rc = ble_gatts_reset();
        if(rc != 0)
        {
            TLS_BT_APPL_TRACE_ERROR("tls_ble_server_demo_api_init failed rc=%d\r\n", rc);
            return rc;
        }

        //step 1: config/adding  the services
        rc = wm_ble_server_demo_gatt_svr_init();

		if(rc == 0)
		{	
		    tls_ble_register_gap_evt(WM_BLE_GAP_EVENT_CONNECT|WM_BLE_GAP_EVENT_DISCONNECT|WM_BLE_GAP_EVENT_NOTIFY_TX|WM_BLE_GAP_EVENT_SUBSCRIBE|WM_BLE_GAP_EVENT_MTU|WM_BLE_GAP_EVENT_REPEAT_PAIRING, ble_gap_evt_cb);
			TLS_BT_APPL_TRACE_DEBUG("### wm_ble_server_api_demo_init \r\n");
            
            g_ble_uart_output_fptr = output_func_ptr;
            /*step 2: start the service*/
            rc = ble_gatts_start();
            assert(rc == 0);
            
            /*step 3: start advertisement*/
            rc = wm_ble_server_api_demo_adv(true); 
            
            if(rc == 0)
            {
                g_ble_server_state = BLE_SERVER_MODE_ADVERTISING;
            }
		}else
		{
			TLS_BT_APPL_TRACE_ERROR("### wm_ble_server_api_demo_init failed(rc=%d)\r\n", rc);
		}
    }
    else
    {
    	TLS_BT_APPL_TRACE_WARNING("wm_ble_server_api_demo_init registered\r\n");
        rc = BLE_HS_EALREADY;
    }
	
	return rc;
}
int tls_ble_server_demo_api_deinit()
{
   int rc = BLE_HS_EAPP;

    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }

   TLS_BT_APPL_TRACE_DEBUG("%s, state=%d\r\n", __FUNCTION__, g_ble_server_state);
   
   if(g_ble_server_state == BLE_SERVER_MODE_CONNECTED || g_ble_server_state == BLE_SERVER_MODE_INDICATING)
   {
        g_ble_demo_indicate_enable = 0;
        
        rc = ble_gap_terminate(g_ble_demo_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if(rc == 0)
        {
           g_ble_server_state = BLE_SERVER_MODE_EXITING; 
        }
   }else if(g_ble_server_state == BLE_SERVER_MODE_ADVERTISING)
   {
        rc = tls_nimble_gap_adv(WM_BLE_ADV_STOP, 0);
        if(rc == 0)
        {
            if(g_ble_uart_output_fptr)
            {
                g_ble_uart_output_fptr = NULL;
            }
            g_send_pending = 0;
            g_ble_server_state = BLE_SERVER_MODE_IDLE;
        }
   }else if(g_ble_server_state == BLE_SERVER_MODE_IDLE)
   {
        rc = 0;
   }else
   {
        rc = BLE_HS_EALREADY;
   }

   return rc;
}
uint32_t tls_ble_server_demo_api_get_mtu()
{
    return g_mtu;
}
int tls_ble_server_demo_api_send_msg(uint8_t *data, int data_len)
{
    int rc;
    struct os_mbuf *om;
    
    //TLS_BT_APPL_TRACE_DEBUG("### %s len=%d\r\n", __FUNCTION__, data_len);

    if(g_send_pending) return BLE_HS_EBUSY;

    if(data_len<=0 || data == NULL)
    {
        return BLE_HS_EINVAL;
    }
    
    om = ble_hs_mbuf_from_flat(data, data_len);
    if (!om) {
        return BLE_HS_ENOMEM;
    }
    
    rc = ble_gattc_indicate_custom(g_ble_demo_conn_handle,g_ble_demo_attr_indicate_handle, om); 
    if(rc == 0)
    {
        g_send_pending = 1;
    }
    return rc;
}

#endif


