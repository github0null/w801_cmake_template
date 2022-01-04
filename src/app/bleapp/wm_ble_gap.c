/*****************************************************************************
**
**  Name:           wm_ble_gap.c
**
**  Description:    This file contains the simply functions between nimble host and tuya hal layer
**
*****************************************************************************/

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "wm_bt_config.h"

#if (WM_NIMBLE_INCLUDED == CFG_ON)

#include "wm_mem.h"
#include "list.h"
#include "wm_bt.h"
#include "wm_bt_app.h"
#include "wm_bt_util.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "wm_ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "wm_bt_storage.h"

/*
 * STRUCTURE DEFINITIONS
 ****************************************************************************************
 */

typedef struct {
	struct dl_list list;
	uint32_t evt;
	app_gap_evt_cback_t *reg_func_ptr;
    struct ble_npl_mutex list_mutex;
} report_evt_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

static report_evt_t report_evt_list;
static struct ble_gap_disc_params disc_params_dft;
static struct ble_gap_adv_params adv_params_dft;
static ble_addr_t   direct_adv_addr;
/*
 * LOCAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */



/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


static int
gap_event(struct ble_gap_event *event, void *arg)
{
    int rc = 0;
    uint32_t evt = (0x01)<<event->type;
    report_evt_t *report_evt = NULL;
    report_evt_t *report_evt_next = NULL;

    TLS_BT_APPL_TRACE_EVENT("gap_event, [%s]\n",tls_bt_gap_evt_2_str(event->type));

    /*Notify those who cares the event type*/
    ble_npl_mutex_pend(&report_evt_list.list_mutex, 0); 
    
    if(!dl_list_empty(&report_evt_list.list))
    {
        dl_list_for_each_safe(report_evt,report_evt_next, &report_evt_list.list, report_evt_t, list)
        {
            ble_npl_mutex_release(&report_evt_list.list_mutex);
            
            if((report_evt)&&(report_evt->evt&evt)&&(report_evt->reg_func_ptr))
            {
                rc = report_evt->reg_func_ptr(event, arg);
            }
            
            ble_npl_mutex_pend(&report_evt_list.list_mutex, 0); 
        }
    }
    ble_npl_mutex_release(&report_evt_list.list_mutex);

    return rc;
}
/**
 * Initiates the GAP general discovery procedure.
 */
static int
scan_enable(uint8_t type, bool filter_duplicate)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        TLS_BT_APPL_TRACE_ERROR("error determining address type; rc=%d\r\n", rc);
        return rc;
    }
    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    if(filter_duplicate)
    {
        disc_params.filter_duplicates = 1;
    }else
    {
        disc_params.filter_duplicates = 0;
    }

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.  1 passive; 0 active;
     */
    disc_params.passive = type;

    /* Use defaults for the rest of the parameters. */
    if(disc_params_dft.itvl)
    {
        disc_params.itvl = disc_params_dft.itvl;
    }else
    {
        disc_params.itvl = 0x40;
    }

    if(disc_params_dft.window)
    {
        disc_params.window = disc_params_dft.window;
    }else
    {
        disc_params.window = 0x20;
    }

    disc_params.filter_policy = disc_params_dft.filter_policy;
    disc_params.limited = disc_params_dft.limited;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      gap_event, NULL);
    if (rc != 0) {
        TLS_BT_APPL_TRACE_ERROR("Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }

    return rc;
}

static void tls_ble_gap_free_left_report_list()
{
	report_evt_t *evt = NULL;
	report_evt_t *evt_next = NULL;	

	if(dl_list_empty(&report_evt_list.list))
		return ;
	
	ble_npl_mutex_pend(&report_evt_list.list_mutex, 0);

	dl_list_for_each_safe(evt, evt_next,&report_evt_list.list, report_evt_t, list)
	{
		dl_list_del(&evt->list);
		tls_mem_free(evt);
	}

	ble_npl_mutex_release(&report_evt_list.list_mutex);    
}

/*
 * EXPORTED FUNCTION DEFINITIONS
 ****************************************************************************************
 */

int tls_nimble_gap_adv(wm_ble_adv_type_t type, int duration)
{
    int rc ;
    
    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }
    
    if(type)
    {
        uint8_t own_addr_type;
        struct ble_gap_adv_params adv_params;
        
        /* Figure out address to use while advertising (no privacy for now) */
        rc = ble_hs_id_infer_auto(0, &own_addr_type);
        if (rc != 0) {
            TLS_BT_APPL_TRACE_ERROR("error determining address type; rc=%d\r\n", rc);
            return rc;
        }

        /* set adv parameters */
        memset(&adv_params, 0, sizeof(adv_params));

        adv_params.conn_mode = adv_params_dft.conn_mode;
        adv_params.disc_mode = adv_params_dft.disc_mode;
        adv_params.high_duty_cycle = adv_params_dft.high_duty_cycle;
        adv_params.channel_map = adv_params_dft.channel_map;
        adv_params.filter_policy = adv_params_dft.filter_policy;

        if(adv_params_dft.itvl_min)
        {
            adv_params.itvl_min = adv_params_dft.itvl_min;
        }else
        {
            adv_params.itvl_min = 0x40;
        }
        
        if(adv_params_dft.itvl_max)
        {
            adv_params.itvl_max = adv_params_dft.itvl_max;
        }else
        {
            adv_params.itvl_max = 0x40;
        }


        TLS_BT_APPL_TRACE_DEBUG("Starting advertising\r\n");

        
        /* As own address type we use hard-coded value, because we generate
              NRPA and by definition it's random */
        rc = ble_gap_adv_start(own_addr_type, &direct_adv_addr, duration?duration:BLE_HS_FOREVER,
                               &adv_params, gap_event, NULL);
        //assert(rc == 0);
        if(rc)
        {
           TLS_BT_APPL_TRACE_WARNING("Starting advertising failed, rc=%d\r\n", rc); 
        }

    }else
    {
        TLS_BT_APPL_TRACE_DEBUG("Stop advertising\r\n");
        rc = ble_gap_adv_stop();
    }

    return rc;
}
static wm_ble_scan_type_t g_scan_state = WM_BLE_SCAN_STOP;

int tls_ble_gap_scan(wm_ble_scan_type_t type, bool filter_duplicate)
{
    int rc = 1;
    
    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }
    
    if((type == WM_BLE_SCAN_STOP) && (g_scan_state != type))
    {
        rc = ble_gap_disc_cancel();
        
    }else if((type == WM_BLE_SCAN_PASSIVE)&&(g_scan_state == WM_BLE_SCAN_STOP))
    {
        /*passive scan*/
        rc = scan_enable(1, filter_duplicate);

    }else if((type == WM_BLE_SCAN_ACTIVE) &&(g_scan_state == WM_BLE_SCAN_STOP))
    {
        /*active scan*/
        rc = scan_enable(0,filter_duplicate);
    }
    
    if(rc == 0)
    {
        g_scan_state = type;
    }

    return rc;
}



int tls_ble_register_gap_evt(uint32_t evt_type, app_gap_evt_cback_t *evt_cback)
{
    int rc = 0;
	report_evt_t *evt = NULL;
    
    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }

    
    ble_npl_mutex_pend(&report_evt_list.list_mutex, 0);	

	dl_list_for_each(evt, &report_evt_list.list, report_evt_t, list)
	{
		if(evt->reg_func_ptr == evt_cback)
		{
			if(evt->evt&evt_type)
			{
				/*Already in the list, do nothing*/
                ble_npl_mutex_release(&report_evt_list.list_mutex);
				return rc;
			}else
			{
				/*Appending this evt to monitor list*/
				evt->evt |= evt_type;
                ble_npl_mutex_release(&report_evt_list.list_mutex);
				return rc;
			}
		}
	}
    ble_npl_mutex_release(&report_evt_list.list_mutex);

	evt = tls_mem_alloc(sizeof(report_evt_t));
	if(evt == NULL)
	{
		return BLE_HS_ENOMEM;
	}

	memset(evt, 0, sizeof(report_evt_t));
	evt->reg_func_ptr = evt_cback;
	evt->evt = evt_type;

    ble_npl_mutex_pend(&report_evt_list.list_mutex, 0);
    dl_list_add_tail(&report_evt_list.list, &evt->list);
    ble_npl_mutex_release(&report_evt_list.list_mutex);
	
	return rc;

}

int tls_ble_gap_set_data(wm_ble_gap_data_t type, uint8_t *data, int data_len)
{
    int rc;
    
    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }

    
    if(type == WM_BLE_ADV_DATA)
    {
        rc = ble_gap_adv_set_data(data,data_len);
    }else if(type == WM_BLE_ADV_RSP_DATA)
    {
        rc = ble_gap_adv_rsp_set_data(data,data_len);
    }else
    {
        rc = -1;
    }

    return rc;
}

int tls_ble_gap_set_name(const char *dev_name, uint8_t update_flash)
{
    int ret;
    assert(dev_name != NULL);

    /*config host stack device name*/
    ble_svc_gap_device_name_set(dev_name);

    /*Update ram information and flush it if needed*/
    if(update_flash)
    {
        ret = btif_config_set_str("Local", "Adapter", "Name", dev_name);
        if(ret)
        {
            ret = btif_config_flush(update_flash);
            return 0;
        }
    }
    return ret;
}
int tls_ble_gap_get_name(char *dev_name)
{
    int ret;
    int ret_size = 0;
    assert(dev_name != NULL);

    /*Update ram information and flush it if needed*/
    ret = btif_config_get_str("Local", "Adapter", "Name", dev_name, &ret_size);
    if(!ret)
    {
        memset(dev_name, 0, 16);
        const char *p = ble_svc_gap_device_name();
        strncpy(dev_name, p, 16);
        ret_size = strlen(dev_name);
    }

    return ret_size;
}
#define BLE_ISVALID_PARAM(x, min, max)  ((x) >= (min) && (x) <= (max))

int tls_ble_gap_set_adv_param(uint8_t adv_type, uint32_t min, uint32_t max, uint8_t chn_map, uint8_t filter_policy,uint8_t *dir_mac,uint8_t dir_mac_type)
{
    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }


    /*!!!make sure the param is valid!!!*/
    if(!BLE_ISVALID_PARAM(min, 0x20, 0x4000) || !BLE_ISVALID_PARAM(max, 0x20, 0x4000))
    {
        return BLE_HS_EINVAL;
    }
    if(min > max)
    {
        return BLE_HS_EINVAL;
    }

    if((chn_map&0x07) == 0x00)
    {
        return BLE_HS_EINVAL;
    }

    if(filter_policy > 0x03)
    {
        return BLE_HS_EINVAL;
    }

    if(dir_mac_type > 0x01)
    {
        return BLE_HS_EINVAL;
    }

    memset(&adv_params_dft, 0, sizeof(adv_params_dft));


    switch(adv_type)
    {
        case WM_BLE_ADV_IND:
                adv_params_dft.conn_mode = BLE_GAP_CONN_MODE_UND;
                adv_params_dft.disc_mode = BLE_GAP_DISC_MODE_GEN;
            break;
        case WM_BLE_ADV_NONCONN_IND:
                adv_params_dft.conn_mode = BLE_GAP_CONN_MODE_NON;
                adv_params_dft.disc_mode = BLE_GAP_DISC_MODE_NON;
            break;
        case WM_BLE_ADV_SCAN_IND:
                adv_params_dft.conn_mode = BLE_GAP_CONN_MODE_NON;
                adv_params_dft.disc_mode = BLE_GAP_DISC_MODE_GEN; // LTD same as GEN;
            break;
        case WM_BLE_ADV_DIRECT_IND_HDC:
                adv_params_dft.high_duty_cycle = 1;
                //passthrough;
        case WM_BLE_ADV_DIRECT_IND_LDC:
                adv_params_dft.conn_mode = BLE_GAP_CONN_MODE_DIR;
                adv_params_dft.disc_mode = BLE_GAP_DISC_MODE_GEN;

                if(ble_addr_cmp(&direct_adv_addr, BLE_ADDR_ANY) == 0)
                {
                    return BLE_HS_EINVAL;
                }
            break;
       default:
                /**/
                adv_params_dft.conn_mode = BLE_GAP_CONN_MODE_UND;
                adv_params_dft.disc_mode = BLE_GAP_DISC_MODE_GEN;
            break;
    }    

    adv_params_dft.itvl_min = min;
    adv_params_dft.itvl_max = max;
    adv_params_dft.channel_map = chn_map&0x07;
    adv_params_dft.filter_policy = filter_policy;

    if(dir_mac)
    {
        direct_adv_addr.type = dir_mac_type;
        memcpy(direct_adv_addr.val, dir_mac, 6);
    }

    return 0;
}
int tls_ble_gap_set_scan_param(uint32_t window, uint32_t intv, uint8_t filter_policy, bool limited, bool passive, bool filter_duplicate)
{
    memset(&disc_params_dft, 0, sizeof(disc_params_dft));

    disc_params_dft.itvl = intv;
    disc_params_dft.window = window;
    disc_params_dft.filter_policy = filter_policy;
    if(limited) disc_params_dft.limited = 1;
    if(passive) disc_params_dft.passive = 1;
    if(filter_duplicate) disc_params_dft.filter_duplicates = 1;

    return 0;
}

int tls_ble_deregister_gap_evt(uint32_t evt_type, app_gap_evt_cback_t *evt_cback)
{
	report_evt_t *evt = NULL;
	report_evt_t *evt_next = NULL;
    
    if(bt_adapter_state == WM_BT_STATE_OFF)
    {
        TLS_BT_APPL_TRACE_ERROR("%s failed rc=%s\r\n", __FUNCTION__, tls_bt_rc_2_str(BLE_HS_EDISABLED));
        return BLE_HS_EDISABLED;
    }

	ble_npl_mutex_pend(&report_evt_list.list_mutex, 0);	
    if(!dl_list_empty(&report_evt_list.list))
    {
    	dl_list_for_each_safe(evt,evt_next, &report_evt_list.list, report_evt_t, list)
    	{
    	    ble_npl_mutex_release(&report_evt_list.list_mutex);
    		if((evt->reg_func_ptr == evt_cback))
    		{
    			evt->evt &= ~evt_type; //clear monitor bit;

    			if(evt->evt == 0)    //no evt left;
    			{
    				dl_list_del(&evt->list);
    				tls_mem_free(evt);
                    evt = NULL;
    			}
    		}
            ble_npl_mutex_pend(&report_evt_list.list_mutex, 0);	
    	}
    }
	ble_npl_mutex_release(&report_evt_list.list_mutex);
	
	return 0;
}

int tls_ble_gap_deinit(void)
{
    /*TODO how to release the character/service list?*/
    /**only be called when bluetooth system turning off, the character/service list is freed by ble_hs_stop() */

    /*Do not forget to release  report event list*/
    tls_ble_gap_free_left_report_list();
    ble_npl_mutex_deinit(&report_evt_list.list_mutex);

    return 0;
}

int tls_ble_gap_init(void)
{
    char default_device_name[MYNEWT_VAL(BLE_SVC_GAP_DEVICE_NAME_MAX_LENGTH)];
    uint8_t bt_mac[6];
    int ret_len = 0;
    
    g_scan_state = WM_BLE_SCAN_STOP;
    memset(&adv_params_dft, 0, sizeof(adv_params_dft)); 
    adv_params_dft.conn_mode = BLE_GAP_CONN_MODE_UND;  //default conn  mode;
    adv_params_dft.disc_mode = BLE_GAP_DISC_MODE_GEN;  //default disc  mode;
    memset(&disc_params_dft, 0, sizeof(disc_params_dft));
    memset(&direct_adv_addr, 0, sizeof(direct_adv_addr));
    dl_list_init(&report_evt_list.list);
    ble_npl_mutex_init(&report_evt_list.list_mutex);
    
    if(btif_config_get_str("Local", "Adapter", "Name", default_device_name, &ret_len))
    {
        ble_svc_gap_device_name_set(default_device_name);
    }else
    {
        tls_get_bt_mac_addr(bt_mac);
        sprintf(default_device_name, "WM-%02X:%02X:%02X", bt_mac[3], bt_mac[4], bt_mac[5]);
        ble_svc_gap_device_name_set(default_device_name);
    }
    return 0;
}
#endif
