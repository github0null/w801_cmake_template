/*
* Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include "sysinit/sysinit.h"
#include "nimble/hci_common.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/ble_hci_trans.h"
#include "wm_bt.h"
#include "wm_mem.h"

#define BLE_HCI_UART_H4_NONE        0x00
#define BLE_HCI_UART_H4_CMD         0x01
#define BLE_HCI_UART_H4_ACL         0x02
#define BLE_HCI_UART_H4_SCO         0x03
#define BLE_HCI_UART_H4_EVT         0x04
#define BLE_HCI_UART_H4_SYNC_LOSS   0x80
#define BLE_HCI_UART_H4_SKIP_CMD    0x81
#define BLE_HCI_UART_H4_SKIP_ACL    0x82
#define BLE_HCI_UART_H4_LE_EVT      0x83
#define BLE_HCI_UART_H4_SKIP_EVT    0x84

#define BLE_HCI_EVENT_HDR_LEN               (2)
#define BLE_HCI_CMD_HDR_LEN                 (3)

static volatile int ble_hci_cmd_pkts = 0;
static ble_hci_trans_rx_cmd_fn *ble_hci_rx_cmd_hs_cb;
static void *ble_hci_rx_cmd_hs_arg;
static ble_hci_trans_rx_acl_fn *ble_hci_rx_acl_hs_cb;
static void *ble_hci_rx_acl_hs_arg;
static struct os_mempool ble_hci_vuart_evt_hi_pool;

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
static os_membuf_t *ble_hci_vuart_evt_hi_buf = NULL;

#else
static os_membuf_t ble_hci_vuart_evt_hi_buf[
        OS_MEMPOOL_SIZE(MYNEWT_VAL(BLE_HCI_EVT_HI_BUF_COUNT),
                        MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE))
];
#endif


static struct os_mempool ble_hci_vuart_evt_lo_pool;

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
static os_membuf_t *ble_hci_vuart_evt_lo_buf = NULL;
#else
static os_membuf_t ble_hci_vuart_evt_lo_buf[
        OS_MEMPOOL_SIZE(MYNEWT_VAL(BLE_HCI_EVT_LO_BUF_COUNT),
                        MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE))
];
#endif

static struct os_mempool ble_hci_vuart_cmd_pool;

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
static os_membuf_t *ble_hci_vuart_cmd_buf = NULL;
#else
static os_membuf_t ble_hci_vuart_cmd_buf[
	OS_MEMPOOL_SIZE(1, BLE_HCI_TRANS_CMD_SZ)
];
#endif

static struct os_mbuf_pool ble_hci_vuart_acl_mbuf_pool;
static struct os_mempool_ext ble_hci_vuart_acl_pool;

/*
 * The MBUF payload size must accommodate the HCI data header size plus the
 * maximum ACL data packet length. The ACL block size is the size of the
 * mbufs we will allocate.
 */
#define ACL_BLOCK_SIZE  OS_ALIGN(MYNEWT_VAL(BLE_ACL_BUF_SIZE) \
                                 + BLE_MBUF_MEMBLOCK_OVERHEAD \
                                 + BLE_HCI_DATA_HDR_SZ, OS_ALIGNMENT)

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
static os_membuf_t *ble_hci_vuart_acl_buf = NULL;
#else
static os_membuf_t ble_hci_vuart_acl_buf[
	OS_MEMPOOL_SIZE(MYNEWT_VAL(BLE_ACL_BUF_COUNT),
                        ACL_BLOCK_SIZE)
];
#endif


//#define HCI_DBG TRUE


#ifndef HCI_DBG
#define HCI_DBG TRUE
#endif

#if (HCI_DBG == TRUE)
#define HCIDBG(fmt, ...)  \
    do{\
        if(1) \
            printf("%s(L%d): " fmt, __FUNCTION__, __LINE__,  ## __VA_ARGS__); \
    }while(0)
#else
#define HCIDBG(param, ...)
#endif


#if (HCI_DBG == TRUE)
static void  hci_dbg_hexstring(const char *msg, const uint8_t *ptr, int a_length)
{
#define DBG_TRACE_WARNING_MAX_SIZE_W  252
    //123
#define DBG_TRACE_WARNING_MAX_SIZE    256
    //128
    char sbuffer[DBG_TRACE_WARNING_MAX_SIZE];
    uint8_t offset = 0;
    int i = 0;
    int length = 0;

    if(msg)
    {
        printf("%s", msg);
    }

    if(a_length <= 0 || ptr == NULL)
    {
        return;
    }

    //length = MIN(40, a_length);
    length = a_length;

    do
    {
        for(; i < length; i++)
        {
            offset += sprintf(sbuffer + offset, "%02X ", (uint8_t)ptr[i]);

            if(offset > DBG_TRACE_WARNING_MAX_SIZE_W)
            {
                break;
            }
        }

        sbuffer[offset - 1] = '\r';
        sbuffer[offset] = '\n';
        sbuffer[offset+1] = 0;

        if(offset > DBG_TRACE_WARNING_MAX_SIZE_W)
        {
            sbuffer[offset - 2] = '.';
            sbuffer[offset - 3] = '.';
        }

        printf("%s", sbuffer);
        offset = 0;
    } while(i < length);
}
#endif


void ble_hci_trans_cfg_hs(ble_hci_trans_rx_cmd_fn *cmd_cb,
                     void *cmd_arg,
                     ble_hci_trans_rx_acl_fn *acl_cb,
                     void *acl_arg)
{
    ble_hci_rx_cmd_hs_cb = cmd_cb;
    ble_hci_rx_cmd_hs_arg = cmd_arg;
    ble_hci_rx_acl_hs_cb = acl_cb;
    ble_hci_rx_acl_hs_arg = acl_arg;
}
int ble_hci_trans_hs_cmd_tx(uint8_t *cmd)
{
    uint16_t len;

    assert(cmd != NULL);
    *cmd = BLE_HCI_UART_H4_CMD;
    len = BLE_HCI_CMD_HDR_LEN + cmd[3] + 1;

    while (ble_hci_cmd_pkts <= 0) {
        tls_os_time_delay(1000 / /*configTICK_RATE_HZ*/HZ);
    }

    //hci_dbg_hexstring(">>>", cmd, len);

    tls_bt_vuart_host_send_packet(cmd, len);
    ble_hci_trans_buf_free(cmd);
    
    return 0;
}

int ble_hci_trans_ll_evt_tx(uint8_t *hci_ev)
{
    int rc = -1;

    if (ble_hci_rx_cmd_hs_cb) {
        rc = ble_hci_rx_cmd_hs_cb(hci_ev, ble_hci_rx_cmd_hs_arg);
    }
    return rc;
}

int ble_hci_trans_hs_acl_tx(struct os_mbuf *om)
{
    uint16_t len = 0;
    uint8_t data[MYNEWT_VAL(BLE_ACL_BUF_SIZE) + 1];
    
    /* If this packet is zero length, just free it */
    if (OS_MBUF_PKTLEN(om) == 0) {
        os_mbuf_free_chain(om);
        return 0;
    }
    data[0] = BLE_HCI_UART_H4_ACL;
    len++;

    while (ble_hci_cmd_pkts <= 0) {
        tls_os_time_delay(1000 / /*configTICK_RATE_HZ*/HZ);
        
    }

    os_mbuf_copydata(om, 0, OS_MBUF_PKTLEN(om), &data[1]);
    len += OS_MBUF_PKTLEN(om);

    tls_bt_vuart_host_send_packet(data, len);
    //hci_dbg_hexstring(">>>", data, len);
    os_mbuf_free_chain(om);

    return 0;
}

int ble_hci_trans_ll_acl_tx(struct os_mbuf *om)
{
    int rc = -1;

    if (ble_hci_rx_acl_hs_cb) {
        rc = ble_hci_rx_acl_hs_cb(om, ble_hci_rx_acl_hs_arg);
    }
    return rc;
}

uint8_t *ble_hci_trans_buf_alloc(int type)
{
    uint8_t *buf;

    switch (type) {
    case BLE_HCI_TRANS_BUF_CMD:
        buf = os_memblock_get(&ble_hci_vuart_cmd_pool);
        break;

    case BLE_HCI_TRANS_BUF_EVT_HI:
        buf = os_memblock_get(&ble_hci_vuart_evt_hi_pool);
        if (buf == NULL) {
            /* If no high-priority event buffers remain, try to grab a
             * low-priority one.
             */
            buf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_LO);
        }
        break;

    case BLE_HCI_TRANS_BUF_EVT_LO:
        buf = os_memblock_get(&ble_hci_vuart_evt_lo_pool);
        break;

    default:
        assert(0);
        buf = NULL;
    }

    return buf;
}

void ble_hci_trans_buf_free(uint8_t *buf)
{
    int rc;

    /*
     * XXX: this may look a bit odd, but the controller uses the command
     * buffer to send back the command complete/status as an immediate
     * response to the command. This was done to insure that the controller
     * could always send back one of these events when a command was received.
     * Thus, we check to see which pool the buffer came from so we can free
     * it to the appropriate pool
     */
    if (os_memblock_from(&ble_hci_vuart_evt_hi_pool, buf)) {
        rc = os_memblock_put(&ble_hci_vuart_evt_hi_pool, buf);
        assert(rc == 0);
    } else if (os_memblock_from(&ble_hci_vuart_evt_lo_pool, buf)) {
        rc = os_memblock_put(&ble_hci_vuart_evt_lo_pool, buf);
        assert(rc == 0);
    } else {
        assert(os_memblock_from(&ble_hci_vuart_cmd_pool, buf));
        rc = os_memblock_put(&ble_hci_vuart_cmd_pool, buf);
        assert(rc == 0);
    }

}

/**
 * Unsupported; the RAM transport does not have a dedicated ACL data packet
 * pool.
 */
int ble_hci_trans_set_acl_free_cb(os_mempool_put_fn *cb, void *arg)
{
    return BLE_ERR_UNSUPPORTED;
}

int ble_hci_trans_reset(void)
{
    /* No work to do.  All allocated buffers are owned by the host or
     * controller, and they will get freed by their owners.
     */
    return 0;
}

/**
 * Allocates a buffer (mbuf) for ACL operation.
 *
 * @return                      The allocated buffer on success;
 *                              NULL on buffer exhaustion.
 */
static struct os_mbuf *ble_hci_trans_acl_buf_alloc(void)
{
    struct os_mbuf *m;
    uint8_t usrhdr_len;

#if MYNEWT_VAL(BLE_DEVICE)
    usrhdr_len = sizeof(struct ble_mbuf_hdr);
#elif MYNEWT_VAL(BLE_HS_FLOW_CTRL)
    usrhdr_len = BLE_MBUF_HS_HDR_LEN;
#else
    usrhdr_len = 0;
#endif

    m = os_mbuf_get_pkthdr(&ble_hci_vuart_acl_mbuf_pool, usrhdr_len);
    return m;
}

static void ble_hci_rx_acl(uint8_t *data, uint16_t len)
{
    struct os_mbuf *m;
    int sr;
    
    if (len < BLE_HCI_DATA_HDR_SZ || len > MYNEWT_VAL(BLE_ACL_BUF_SIZE)) {
        assert(0);
        return;
    }

    m = ble_hci_trans_acl_buf_alloc();

    if (!m) {
        assert(0);
        return;
    }
    if (os_mbuf_append(m, data, len)) {
        assert(0);
        os_mbuf_free_chain(m);
        return;
    }
    OS_ENTER_CRITICAL(sr);
    if (ble_hci_rx_acl_hs_cb) {
        ble_hci_rx_acl_hs_cb(m, NULL);
    }
    OS_EXIT_CRITICAL(sr);
}

void notify_host_send_available(int cnt) {
    ble_hci_cmd_pkts = cnt;
}

static int notify_host_recv(uint8_t *data, uint16_t len)
{
    //hci_dbg_hexstring("<<<", data, len);
    if (data[0] == BLE_HCI_UART_H4_EVT) {
        uint8_t *evbuf;
        int totlen;
        int rc;

        totlen = BLE_HCI_EVENT_HDR_LEN + data[2];
        assert(totlen <= UINT8_MAX + BLE_HCI_EVENT_HDR_LEN);

        if (data[1] == BLE_HCI_EVCODE_HW_ERROR) {
            assert(0);
        }

        /* Allocate LE Advertising Report Event from lo pool only */
        if ((data[1] == BLE_HCI_EVCODE_LE_META) && (data[3] == BLE_HCI_LE_SUBEV_ADV_RPT)) {
            evbuf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_LO);
            /* Skip advertising report if we're out of memory */
            if (!evbuf) {
                return 0;
            }
        } else {
            evbuf = ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_EVT_HI);
            assert(evbuf != NULL);
        }

        memcpy(evbuf, &data[1], totlen);

        rc = ble_hci_trans_ll_evt_tx(evbuf);
        assert(rc == 0);
    } else if (data[0] == BLE_HCI_UART_H4_ACL) {
        ble_hci_rx_acl(data + 1, len - 1);
    }
    return 0;
}

static tls_bt_host_if_t vuart_hci_cb = {
    .notify_controller_avaiable_hci_buffer = notify_host_send_available,
    .notify_host_recv_h4 = notify_host_recv
};


static int wm_ble_controller_init(uint8_t uart_idx)
{
    tls_bt_hci_if_t hci_if;
    tls_bt_status_t status;
    
    hci_if.uart_index = uart_idx;
    status = tls_bt_ctrl_enable(&hci_if, 0);

    if((status != TLS_BT_STATUS_SUCCESS) && (status != TLS_BT_STATUS_DONE))
    {
        return TLS_BT_STATUS_CTRL_ENABLE_FAILED;
    }
    
    status = tls_bt_ctrl_if_register(&vuart_hci_cb);
    
    if(status != TLS_BT_STATUS_SUCCESS)
    {
        return TLS_BT_STATUS_CTRL_ENABLE_FAILED; 
    }

    return 0;

}
static int wm_ble_controller_deinit()
{
    return tls_bt_ctrl_disable();
}

/**
 * Initializes the VUART HCI transport module.
 *
 * @return                      0 on success;
 *                              A BLE_ERR_[...] error code on failure.
 */
void
ble_hci_vuart_init(uint8_t uart_idx)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();
    
#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
    ble_hci_vuart_acl_buf = (os_membuf_t *)tls_mem_alloc(
        sizeof(os_membuf_t) *
        OS_MEMPOOL_SIZE(MYNEWT_VAL(BLE_ACL_BUF_COUNT),ACL_BLOCK_SIZE));
    assert(ble_hci_vuart_acl_buf != NULL);
#endif

    rc = os_mempool_ext_init(&ble_hci_vuart_acl_pool,
                             MYNEWT_VAL(BLE_ACL_BUF_COUNT),
                             ACL_BLOCK_SIZE,
                             ble_hci_vuart_acl_buf,
                             "ble_hci_vuart_acl_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = os_mbuf_pool_init(&ble_hci_vuart_acl_mbuf_pool,
                           &ble_hci_vuart_acl_pool.mpe_mp,
                           ACL_BLOCK_SIZE,
                           MYNEWT_VAL(BLE_ACL_BUF_COUNT));
    SYSINIT_PANIC_ASSERT(rc == 0);

    /*
     * Create memory pool of HCI command buffers. NOTE: we currently dont
     * allow this to be configured. The controller will only allow one
     * outstanding command. We decided to keep this a pool in case we allow
     * allow the controller to handle more than one outstanding command.
     */

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
    ble_hci_vuart_cmd_buf = (os_membuf_t *)tls_mem_alloc(
        sizeof(os_membuf_t) *
        OS_MEMPOOL_SIZE(1, BLE_HCI_TRANS_CMD_SZ));
    assert(ble_hci_vuart_cmd_buf != NULL);

#endif

    rc = os_mempool_init(&ble_hci_vuart_cmd_pool,
                         1,
                         BLE_HCI_TRANS_CMD_SZ,
                         ble_hci_vuart_cmd_buf,
                         "ble_hci_vuart_cmd_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
    ble_hci_vuart_evt_hi_buf = (os_membuf_t *)tls_mem_alloc(
        sizeof(os_membuf_t) *
        OS_MEMPOOL_SIZE(MYNEWT_VAL(BLE_HCI_EVT_HI_BUF_COUNT),
                    MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE)));
    assert(ble_hci_vuart_evt_hi_buf != NULL);
    
#endif

    rc = os_mempool_init(&ble_hci_vuart_evt_hi_pool,
                         MYNEWT_VAL(BLE_HCI_EVT_HI_BUF_COUNT),
                         MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE),
                         ble_hci_vuart_evt_hi_buf,
                         "ble_hci_vuart_evt_hi_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
    ble_hci_vuart_evt_lo_buf = (os_membuf_t *)tls_mem_alloc(
        sizeof(os_membuf_t) *
        OS_MEMPOOL_SIZE(MYNEWT_VAL(BLE_HCI_EVT_LO_BUF_COUNT),
                MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE)));
    assert(ble_hci_vuart_evt_lo_buf != NULL);
        
#endif

    rc = os_mempool_init(&ble_hci_vuart_evt_lo_pool,
                         MYNEWT_VAL(BLE_HCI_EVT_LO_BUF_COUNT),
                         MYNEWT_VAL(BLE_HCI_EVT_BUF_SIZE),
                         ble_hci_vuart_evt_lo_buf,
                         "ble_hci_vuart_evt_lo_pool");
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = wm_ble_controller_init(uart_idx);
    assert(rc == 0);


}

int ble_hci_vuart_deinit()
{
    int rc; 

    rc = wm_ble_controller_deinit();
    assert(rc == 0);

#if 0
    os_mempool_clear(&ble_hci_vuart_evt_lo_pool);    
    os_mempool_clear(&ble_hci_vuart_evt_hi_pool);    
    os_mempool_clear(&ble_hci_vuart_cmd_pool);    
    os_mempool_clear(&ble_hci_vuart_acl_pool);    
#endif

#if MYNEWT_VAL(SYS_MEM_DYNAMIC)
    if(ble_hci_vuart_acl_buf)
    {
        tls_mem_free(ble_hci_vuart_acl_buf);
        ble_hci_vuart_acl_buf = NULL;
    }

    if(ble_hci_vuart_cmd_buf)
    {
        tls_mem_free(ble_hci_vuart_cmd_buf);
        ble_hci_vuart_cmd_buf = NULL;
    }  

    if(ble_hci_vuart_evt_hi_buf)
    {
        tls_mem_free(ble_hci_vuart_evt_hi_buf);
        ble_hci_vuart_evt_hi_buf = NULL;
    }    

    if(ble_hci_vuart_evt_lo_buf)
    {
        tls_mem_free(ble_hci_vuart_evt_lo_buf);
        ble_hci_vuart_evt_lo_buf = NULL;
    } 
       
#endif
    return rc;
}

