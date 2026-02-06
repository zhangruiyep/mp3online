
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/dns.h"
#include <webclient.h>
#include <cJSON.h>
#if PKG_NETUTILS_NTP
#include "ntp.h"
#endif
#include "mp3_ringbuffer.h"
#include "mp3_network.h"
#include "local_music.h"

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#define GET_HEADER_BUFSZ        2048        //头部大小
#define GET_RESP_BUFSZ          8192        //响应缓冲区大小
#define GET_URL_LEN_MAX         256         //网址最大长度

//#define MP3_DL_TRUNC_SIZE   (MP3_RING_BUFFER_SIZE/2)

typedef void(*mp3_user_cb)(int ret);

typedef enum
{
    MP3_DL_CMD_READ_MORE,
    MP3_DL_CMD_STOP,
} mp3_dl_cmd_t;

typedef struct
{
    mp3_dl_cmd_t cmd;
    union
    {
        int read_pos;
    } data;
} mp3_dl_msg_t;

typedef enum
{
    MP3_DL_STATE_IDLE,
    MP3_DL_STATE_INIT,
    MP3_DL_STATE_DLING,
} mp3_dl_state_t;

static mp3_dl_state_t g_mp3_dl_state = MP3_DL_STATE_IDLE;

static int g_mp3_dl_content_len = 0;
static int g_mp3_dl_content_pos = 0;

static rt_timer_t g_mp3_dl_start_timer = NULL;
static mp3_user_cb g_user_cb = NULL;

/*  */
static int mp3_dl_get_part_callback(uint8_t *data, size_t len)
{
    rt_kprintf("%s %d: len=%d\n", __func__, __LINE__, len);
    if ((data == NULL) || (len == 0) || (len > 50*1000*1000))
    {
        if (g_user_cb)
        {
            g_user_cb(-1);
            g_user_cb = NULL;
        }
        if (g_mp3_dl_start_timer)
        {
            rt_timer_stop(g_mp3_dl_start_timer);
        }
        rt_kprintf("%s %d: input invalid\n", __func__, __LINE__);
        return 0;
    }

    mp3_ring_buffer_put(data, mp3_ring_buffer_space_len());

    g_mp3_dl_state = MP3_DL_STATE_DLING;
    g_mp3_dl_content_len = len;
    g_mp3_dl_content_pos += MP3_RING_BUFFER_SIZE;
    play_ringbuff(g_mp3_dl_content_len);
    if (g_user_cb)
    {
        g_user_cb(0);
        g_user_cb = NULL;
    }
    if (g_mp3_dl_start_timer)
    {
        rt_timer_stop(g_mp3_dl_start_timer);
    }
    return 0;
}

static int mp3_dl_get_part_continue_callback(uint8_t *data, size_t len)
{
    //rt_kprintf("%s %d: len=%d\n", __func__, __LINE__, len);
    if ((data == NULL) || (len == 0))
    {
        return 0;
    }

    mp3_ring_buffer_put(data, len);

    g_mp3_dl_state = MP3_DL_STATE_DLING;
    g_mp3_dl_content_pos += len;
    //rt_kprintf("%s %d: g_mp3_dl_content_pos=%d\n", __func__, __LINE__, g_mp3_dl_content_pos);
    if (g_mp3_dl_content_pos >= g_mp3_dl_content_len)
    {
        /* download done */
        rt_kprintf("%s %d: dl done\n", __func__, __LINE__);
        g_mp3_dl_state = MP3_DL_STATE_IDLE;
    }
    return 0;
}

void mp3_dl_read_more(int read_pos)
{
    //rt_kprintf("%s %d: read_pos=%d\n", __func__, __LINE__, read_pos);
    if (g_mp3_dl_state == MP3_DL_STATE_IDLE)
    {
        rt_kprintf("%s %d: no more data\n", __func__, __LINE__);
        return;
    }

    //int remain_len = mp3_ring_buffer_data_len();
    //rt_kprintf("%s %d: remain_len=%d\n", __func__, __LINE__, remain_len);
    //if (remain_len < MP3_DL_TRUNC_SIZE)
    {
        int ring_space = mp3_ring_buffer_space_len();
        int last = g_mp3_dl_content_len - g_mp3_dl_content_pos;
        //rt_kprintf("%s %d: last=%d\n", __func__, __LINE__, last);
        int dl_len = MIN(ring_space, last);
        //rt_kprintf("%s %d: dl_len=%d\n", __func__, __LINE__, dl_len);
        mp3_network_get_part_continue(dl_len, mp3_dl_get_part_continue_callback);
    }
}

int mp3_dl_thread_init(const char *mp3_url)
{
    int ret = 0;
    rt_kprintf("%s %d: g_mp3_dl_state=%d\n", __func__, __LINE__, g_mp3_dl_state);
    if (g_mp3_dl_state == MP3_DL_STATE_IDLE)
    {
        mp3_ring_buffer_reset();
        g_mp3_dl_content_len = 0;
        g_mp3_dl_content_pos = 0;

        ret = mp3_network_get_part(mp3_url, mp3_ring_buffer_space_len(), mp3_dl_get_part_callback);
        if (ret < 0)
        {
            rt_kprintf("%s %d: ERR ret=%d\n", __func__, __LINE__, ret);
            return ret;
        }
        g_mp3_dl_state = MP3_DL_STATE_INIT;
    }
    else
    {
        rt_kprintf("%s %d: state err=%d\n", __func__, __LINE__, g_mp3_dl_state);
    }
    return ret;
}

void mp3_stream_resume(void)
{
    if (g_mp3_dl_state == MP3_DL_STATE_DLING)
    {
        play_resume();
    }
    else
    {
        rt_kprintf("%s %d: state err=%d\n", __func__, __LINE__, g_mp3_dl_state);
    }
}

void mp3_stream_pause(void)
{
    if (g_mp3_dl_state == MP3_DL_STATE_DLING)
    {
        play_pause();
    }
    else
    {
        rt_kprintf("%s %d: state err=%d\n", __func__, __LINE__, g_mp3_dl_state);
    }
}


void mp3_stream_start_timer_cb(void *parameter)
{
    rt_kprintf("%s %d: wait dl timeout\n", __func__, __LINE__);
    if (g_user_cb)
    {
        g_user_cb(-1);
        g_user_cb = NULL;
    }
    rt_timer_stop(g_mp3_dl_start_timer);
}

void mp3_stream_start(const char *mp3_url, void *user_cb)
{
    int ret = mp3_dl_thread_init(mp3_url);
    if (ret < 0)
    {
        /* can not init mp3 dl, try reboot to recover network */
        drv_reboot();
    }
    g_user_cb = user_cb;
    if (g_mp3_dl_state == MP3_DL_STATE_INIT)
    {
        if (!g_mp3_dl_start_timer)
        {
            g_mp3_dl_start_timer = rt_timer_create("mp3start", mp3_stream_start_timer_cb, NULL,
                                                rt_tick_from_millisecond(30000), RT_TIMER_FLAG_SOFT_TIMER | RT_TIMER_FLAG_ONE_SHOT);
        }
        else
        {
            rt_timer_stop(g_mp3_dl_start_timer);
        }
        rt_timer_start(g_mp3_dl_start_timer);
    }
    else
    {
       rt_kprintf("%s %d: state err=%d\n", __func__, __LINE__, g_mp3_dl_state);
    }
}

void mp3_stream_stop(void)
{
    if (g_mp3_dl_state != MP3_DL_STATE_IDLE)
    {
        mp3_network_get_part_cancel();
        play_stop();
        g_mp3_dl_state = MP3_DL_STATE_IDLE;
    }
}

uint32_t mp3_stream_get_total_seconds(void)
{
    return play_get_total_seconds();
}

#if 0
static void mp3play(int argc, char **argv)
{
    mp3_dl_thread_init("http://music.163.com/song/media/outer/url?id=2155423468.mp3");
    int retry = 30;
    while (retry-- > 0)
    {
        if (g_mp3_dl_content_len)
        {
            play_buff(g_mp3_ring_buffer, g_mp3_dl_content_len);
            break;
        }
        rt_thread_mdelay(1000);
    }
}
MSH_CMD_EXPORT(mp3play, MP3 play online)
#endif

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/

