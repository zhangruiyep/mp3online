/**
  ******************************************************************************
  * @file   main.c
  * @author Sifli software development team
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2024 - 2025,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */



#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "string.h"

/* Common functions for RT-Thread based platform -----------------------------------------------*/
/**
  * @brief  Initialize board default configuration.
  * @param  None
  * @retval None
  */
void HAL_MspInit(void)
{
    //__asm("B .");        /*For debugging purpose*/
    BSP_IO_Init();
}
/* User code start from here --------------------------------------------------------*/
#include "bts2_app_inc.h"
#include "ble_connection_manager.h"
#include "bt_connection_manager.h"

#ifdef OTA_55X
    #include "dfu_service.h"
#endif

#include "ulog.h"


#define BT_APP_READY 1

#define BT_APP_CONNECT_PAN  1
#define PAN_TIMER_MS        3000

#define URL "http://113.204.105.154:19000/ota-file/sdk/offline_install_h.bin"

typedef struct
{
    BOOL bt_connected;
    bt_notify_device_mac_t bd_addr;
    rt_timer_t pan_connect_timer;
} bt_app_t;
static bt_app_t g_bt_app_env;
static rt_mailbox_t g_bt_app_mb;

void bt_app_connect_pan_timeout_handle(void *parameter)
{
    if ((g_bt_app_mb != NULL) && (g_bt_app_env.bt_connected))
        rt_mb_send(g_bt_app_mb, BT_APP_CONNECT_PAN);
    return;
}


#if defined(BSP_USING_SPI_NAND) && defined(RT_USING_DFS)
#include "dfs_file.h"
#include "dfs_posix.h"
#include "drv_flash.h"
#define NAND_MTD_NAME    "root"
int mnt_init(void)
{
    //TODO: how to get base address
    register_nand_device(FS_REGION_START_ADDR & (0xFC000000), FS_REGION_START_ADDR - (FS_REGION_START_ADDR & (0xFC000000)), FS_REGION_SIZE, NAND_MTD_NAME);
    if (dfs_mount(NAND_MTD_NAME, "/", "elm", 0, 0) == 0) // fs exist
    {
        rt_kprintf("mount fs on flash to root success\n");
    }
    else
    {
        // auto mkfs, remove it if you want to mkfs manual
        rt_kprintf("mount fs on flash to root fail\n");
        if (dfs_mkfs("elm", NAND_MTD_NAME) == 0)
        {
            rt_kprintf("make elm fs on flash sucess, mount again\n");
            if (dfs_mount(NAND_MTD_NAME, "/", "elm", 0, 0) == 0)
                rt_kprintf("mount fs on flash success\n");
            else
                rt_kprintf("mount to fs on flash fail\n");
        }
        else
            rt_kprintf("dfs_mkfs elm flash fail\n");
    }
    return RT_EOK;
}
INIT_ENV_EXPORT(mnt_init);
#endif


static int bt_app_interface_event_handle(uint16_t type, uint16_t event_id, uint8_t *data, uint16_t data_len)
{
    if (type == BT_NOTIFY_COMMON)
    {
        int pan_conn = 0;

        switch (event_id)
        {
        case BT_NOTIFY_COMMON_BT_STACK_READY:
        {
            rt_mb_send(g_bt_app_mb, BT_APP_READY);
        }
        break;
        case BT_NOTIFY_COMMON_ACL_DISCONNECTED:
        {
            bt_notify_device_base_info_t *info = (bt_notify_device_base_info_t *)data;
            LOG_I("disconnected(0x%.2x:%.2x:%.2x:%.2x:%.2x:%.2x) res %d", info->mac.addr[5],
                  info->mac.addr[4], info->mac.addr[3], info->mac.addr[2],
                  info->mac.addr[1], info->mac.addr[0], info->res);
            g_bt_app_env.bt_connected = FALSE;
            memset(&g_bt_app_env.bd_addr, 0xFF, sizeof(g_bt_app_env.bd_addr));

            if (g_bt_app_env.pan_connect_timer)
                rt_timer_stop(g_bt_app_env.pan_connect_timer);
        }
        break;
        case BT_NOTIFY_COMMON_ENCRYPTION:
        {
            bt_notify_device_mac_t *mac = (bt_notify_device_mac_t *)data;
            LOG_I("Encryption competed");
            g_bt_app_env.bd_addr = *mac;
            pan_conn = 1;
        }
        break;
        case BT_NOTIFY_COMMON_PAIR_IND:
        {
            bt_notify_device_base_info_t *info = (bt_notify_device_base_info_t *)data;
            LOG_I("Pairing completed %d", info->res);
            if (info->res == BTS2_SUCC)
            {
                g_bt_app_env.bd_addr = info->mac;
                pan_conn = 1;
            }
        }
        break;
        default:
            break;
        }

        if (pan_conn)
        {
            LOG_I("bd addr 0x%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", g_bt_app_env.bd_addr.addr[5],
                  g_bt_app_env.bd_addr.addr[4], g_bt_app_env.bd_addr.addr[3],
                  g_bt_app_env.bd_addr.addr[2], g_bt_app_env.bd_addr.addr[1],
                  g_bt_app_env.bd_addr.addr[0]);
            g_bt_app_env.bt_connected = TRUE;
            // Trigger PAN connection after PAN_TIMER_MS period to avoid SDP confliction.
            if (!g_bt_app_env.pan_connect_timer)
                g_bt_app_env.pan_connect_timer = rt_timer_create("connect_pan", bt_app_connect_pan_timeout_handle, (void *)&g_bt_app_env,
                                                 rt_tick_from_millisecond(PAN_TIMER_MS), RT_TIMER_FLAG_SOFT_TIMER);
            else
                rt_timer_stop(g_bt_app_env.pan_connect_timer);
            rt_timer_start(g_bt_app_env.pan_connect_timer);
        }
    }
    else if (type == BT_NOTIFY_PAN)
    {
        switch (event_id)
        {
        case BT_NOTIFY_PAN_PROFILE_CONNECTED:
        {
            LOG_I("pan connect successed \n");
            if (g_bt_app_env.pan_connect_timer)
            {
                rt_timer_stop(g_bt_app_env.pan_connect_timer);
            }
        }
        break;
        case BT_NOTIFY_PAN_PROFILE_DISCONNECTED:
        {
            LOG_I("pan disconnect with remote device\n");
        }
        break;
        default:
            break;
        }
    }


    return 0;
}

uint32_t bt_get_class_of_device()
{
    return (uint32_t)BT_SRVCLS_NETWORK | BT_DEVCLS_LAP | BT_LAP_FULLY;
}


/**
  * @brief  Main program
  * @param  None
  * @retval 0 if success, otherwise failure number
  */
#ifdef BT_DEVICE_NAME
    static const char *local_name = BT_DEVICE_NAME;
#else
    static const char *local_name = "sifli_pan";
#endif

void bt_pan_thread_entry(void* params)
{
    g_bt_app_mb = rt_mb_create("bt_app", 8, RT_IPC_FLAG_FIFO);
#ifdef BSP_BT_CONNECTION_MANAGER
    bt_cm_set_profile_target(BT_CM_HID, BT_LINK_PHONE, 1);
#endif // BSP_BT_CONNECTION_MANAGER

    bt_interface_register_bt_event_notify_callback(bt_app_interface_event_handle);

    sifli_ble_enable();
    while (1)
    {
        uint32_t value;
        // Wait for stack/profile ready.
        if (RT_EOK == rt_mb_recv(g_bt_app_mb, (rt_uint32_t *)&value, 8000) && value == BT_APP_READY)
            LOG_I("BT/BLE stack and profile ready");
        else
            LOG_I("BT/BLE stack and profile init failed");

        // Update Bluetooth name
        bt_interface_set_local_name(strlen(local_name), (void *)local_name);

        // handle pan connect event
        rt_mb_recv(g_bt_app_mb, (rt_uint32_t *)&value, RT_WAITING_FOREVER);
        if (value == BT_APP_CONNECT_PAN)
        {
            if (g_bt_app_env.bt_connected)
                bt_interface_conn_ext((char *)&g_bt_app_env.bd_addr, BT_PROFILE_PAN);
        }
    }
    return;
}


static void pan_cmd(int argc, char **argv)
{
    if (strcmp(argv[1], "del_bond") == 0)
    {
#ifdef BSP_BT_CONNECTION_MANAGER
        bt_cm_delete_bonded_devs();
        LOG_D("Delete bond");
#endif // BSP_BT_CONNECTION_MANAGER
    }
    // only valid after connection setup but phone didn't enable pernal hop
    else if (strcmp(argv[1], "conn_pan") == 0)
        bt_app_connect_pan_timeout_handle(NULL);
#if BSP_USING_DFU
    else if (strcmp(argv[1], "ota_pan") == 0)
    {
        bt_dfu_pan_download(URL);
    }
#endif
}
MSH_CMD_EXPORT(pan_cmd, Connect PAN to last paired device);


static rt_thread_t g_bt_pan_thread = RT_NULL;
int bt_pan_thread_create(void)
{
    if (g_bt_pan_thread == RT_NULL)
    {
        g_bt_pan_thread = rt_thread_create("bt_pan",
                                    bt_pan_thread_entry,
                                    RT_NULL,
                                    4096,
                                    RT_THREAD_PRIORITY_HIGH,
                                    RT_THREAD_TICK_DEFAULT);
    }

    if (g_bt_pan_thread != RT_NULL)
    {
        rt_thread_startup(g_bt_pan_thread);
    }

    return RT_EOK;
}
