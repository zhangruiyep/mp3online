
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/dns.h"
#include <webclient.h>
#include <cJSON.h>
#if PKG_NETUTILS_NTP
#include "ntp.h"
#endif
#include "mp3_network.h"
#include "local_music.h"

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#define GET_HEADER_BUFSZ        2048        //头部大小
#define GET_RESP_BUFSZ          8192        //响应缓冲区大小
#define GET_URL_LEN_MAX         256         //网址最大长度

//#define MP3_HOST_BASE_URL       "https://music.taihe.com/v1"
//#define MP3_PLAYLIST_API        "/tracklist/info"

#define MP3_RING_BUFFER_SIZE (1024*16)
extern uint8_t g_mp3_ring_buffer[MP3_RING_BUFFER_SIZE];
extern int g_mp3_ring_buffer_write_pos;
extern int g_mp3_ring_buffer_read_pos;

#define MP3_DL_TRUNC_SIZE   (MP3_RING_BUFFER_SIZE/2)

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

static rt_mq_t g_mp3_dl_mq = NULL;
static rt_thread_t g_mp3_dl_thread = NULL;

static rt_timer_t g_mp3_dl_start_timer = NULL;
static mp3_user_cb user_cb = NULL;

/*  */
static int mp3_dl_get_part_callback(uint8_t *data, size_t len)
{
    rt_kprintf("%s %d: len=%d\n", __func__, __LINE__, len);
    if (len == 0)
    {
        return 0;
    }

    g_mp3_dl_state = MP3_DL_STATE_DLING;
    g_mp3_dl_content_len = len;
    g_mp3_dl_content_pos += MP3_RING_BUFFER_SIZE;
    play_buff(g_mp3_ring_buffer, g_mp3_dl_content_len);
    if (user_cb)
    {
        user_cb(0);
    }
    if (g_mp3_dl_start_timer)
    {
        rt_timer_stop(g_mp3_dl_start_timer);
    }
    return 0;
}

static int mp3_dl_get_part_continue_callback(uint8_t *data, size_t len)
{
    rt_kprintf("%s %d: len=%d\n", __func__, __LINE__, len);
    if ((data == NULL) || (len == 0))
    {
        return 0;
    }

    g_mp3_dl_state = MP3_DL_STATE_DLING;
    g_mp3_ring_buffer_write_pos += len;
    if (g_mp3_ring_buffer_write_pos >= MP3_RING_BUFFER_SIZE)
    {
        g_mp3_ring_buffer_write_pos = 0;
    }
    rt_kprintf("%s %d: g_mp3_ring_buffer_write_pos=%d\n", __func__, __LINE__, g_mp3_ring_buffer_write_pos);
    g_mp3_dl_content_pos += len;
    rt_kprintf("%s %d: g_mp3_dl_content_pos=%d\n", __func__, __LINE__, g_mp3_dl_content_pos);
    if (g_mp3_dl_content_pos >= g_mp3_dl_content_len)
    {
        /* download done */
        rt_kprintf("%s %d: dl done\n", __func__, __LINE__);
        g_mp3_dl_state = MP3_DL_STATE_IDLE;
    }
    return 0;
}

static void send_msg_to_mp3_dl(mp3_dl_msg_t *msg)
{
    if (g_mp3_dl_mq)
    {
        rt_err_t err = rt_mq_send(g_mp3_dl_mq, msg, sizeof(mp3_dl_msg_t));
        RT_ASSERT(err == RT_EOK);
    }
}

void send_read_msg_to_mp3_dl(int read_pos)
{
    rt_kprintf("%s in\n", __func__);
    mp3_dl_msg_t msg = {0};
    msg.cmd = MP3_DL_CMD_READ_MORE;
    msg.data.read_pos = read_pos;

    send_msg_to_mp3_dl(&msg);
}

void mp3_dl_read_more(int read_pos)
{
    rt_kprintf("%s %d: data=%d\n", __func__, __LINE__, read_pos);
    if (g_mp3_dl_state == MP3_DL_STATE_IDLE)
    {
        rt_kprintf("%s %d: no more data\n", __func__, __LINE__);
        return;
    }

    g_mp3_ring_buffer_read_pos = read_pos;
    int remain_len = g_mp3_ring_buffer_write_pos - g_mp3_ring_buffer_read_pos;
    if (remain_len < 0)
    {
        remain_len += MP3_RING_BUFFER_SIZE;
    }
    rt_kprintf("%s %d: remain_len=%d\n", __func__, __LINE__, remain_len);
    if (remain_len < MP3_DL_TRUNC_SIZE)
    {
        /* make sure only trigger once */
        static int writing_pos = 0;
        /* make sure dl write in buffer range */
        rt_kprintf("%s %d: g_mp3_ring_buffer_write_pos=%d\n", __func__, __LINE__, g_mp3_ring_buffer_write_pos);
        if (writing_pos != g_mp3_ring_buffer_write_pos)
        {
            /* download still in progress, wait, do not send more request */
            rt_kprintf("%s %d: wait for more data, skip get\n", __func__, __LINE__);
            return;
        }

        int dl_len = MP3_RING_BUFFER_SIZE - g_mp3_ring_buffer_write_pos;
        if (dl_len > MP3_DL_TRUNC_SIZE)
        {
            dl_len = MP3_DL_TRUNC_SIZE;
        }
        /* last packet */
        int last = g_mp3_dl_content_len - g_mp3_dl_content_pos;
        rt_kprintf("%s %d: last=%d\n", __func__, __LINE__, last);
        if (last < MP3_DL_TRUNC_SIZE)
        {
            dl_len = last;
        }
        rt_kprintf("%s %d: dl_len=%d\n", __func__, __LINE__, dl_len);
        mp3_network_get_part_continue(&g_mp3_ring_buffer[g_mp3_ring_buffer_write_pos], dl_len, mp3_dl_get_part_continue_callback);
        writing_pos += dl_len;  //prepare writing, but not done
        if (writing_pos >= MP3_RING_BUFFER_SIZE)
        {
            writing_pos -= MP3_RING_BUFFER_SIZE;
        }
    }
}

void send_stop_msg_to_mp3_dl(void)
{
    rt_kprintf("%s in\n", __func__);
    mp3_dl_msg_t msg = {0};
    msg.cmd = MP3_DL_CMD_STOP;

    send_msg_to_mp3_dl(&msg);
}

void mp3_dl_thread_entry(void *params)
{
    char *buffer = RT_NULL;
    int resp_status;
    struct webclient_session *session = RT_NULL;
    const char *mp3_url = (const char *)params;
    rt_kprintf("%s: mp3_url=%s\n", __func__, mp3_url);
    int content_length = -1, bytes_read = 0;
    int content_pos = 0;

    /* 创建会话并且设置响应的大小 */
    session = webclient_session_create(GET_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        rt_kprintf("No memory for get header!\n");
        goto __exit;
    }

    /* 发送 GET 请求使用默认的头部 */
    if ((resp_status = webclient_get(session, mp3_url)) != 200)
    {
        rt_kprintf("webclient GET request failed, response(%d) error.\n", resp_status);
        /* retry */
        if ((resp_status = webclient_get(session, mp3_url)) != 200)
        {
            rt_kprintf("webclient GET request failed, response(%d) error.\n", resp_status);
            goto __exit;
        }
    }

    content_length = webclient_content_length_get(session);
    if (content_length > 0)
    {
        g_mp3_dl_state = MP3_DL_STATE_DLING;
        rt_kprintf("content_length==%d\n", content_length);
        g_mp3_dl_content_len = content_length;

        bytes_read = webclient_read(session, g_mp3_ring_buffer, MIN(MP3_RING_BUFFER_SIZE, content_length));
        rt_kprintf("first bytes_read=%d\n", bytes_read);
        if (bytes_read <= 0)
        {
            rt_kprintf("%s bytes_read=%d err!!\n", bytes_read);
            goto __exit;
        }
        content_pos += bytes_read;
    }
    else
    {
        rt_kprintf("content_length==0! return NULL\n");
    }

    rt_kprintf("content_pos=%d\n", content_pos);

    mp3_dl_msg_t msg = {0};
    while (content_pos < content_length)
    {
        rt_err_t err = rt_mq_recv(g_mp3_dl_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
        RT_ASSERT(err == RT_EOK);
        //rt_kprintf("%s RECV msg: cmd %d\n", __func__, msg.cmd);
        switch (msg.cmd)
        {
            case MP3_DL_CMD_READ_MORE:
                rt_kprintf("%s %d: data=%d\n", __func__, __LINE__, msg.data.read_pos);
                g_mp3_ring_buffer_read_pos = msg.data.read_pos;
                int remain_len = g_mp3_ring_buffer_write_pos - g_mp3_ring_buffer_read_pos;
                if (remain_len < 0)
                {
                    remain_len += MP3_RING_BUFFER_SIZE;
                }
                rt_kprintf("%s %d: remain_len=%d\n", __func__, __LINE__, remain_len);

                if (remain_len < MP3_RING_BUFFER_SIZE/2)
                {
                    /* make sure dl write in buffer range */
                    rt_kprintf("%s %d: g_mp3_ring_buffer_write_pos=%d\n", __func__, __LINE__, g_mp3_ring_buffer_write_pos);
                    int dl_len = MP3_RING_BUFFER_SIZE - g_mp3_ring_buffer_write_pos;
                    if (dl_len > MP3_RING_BUFFER_SIZE/2)
                    {
                        dl_len = MP3_RING_BUFFER_SIZE/2;
                    }
                    rt_kprintf("%s %d: dl_len=%d\n", __func__, __LINE__, dl_len);

                    bytes_read = webclient_read(session, &g_mp3_ring_buffer[g_mp3_ring_buffer_write_pos], dl_len);
                    if (bytes_read <= 0)
                    {
                        rt_kprintf("%s %d: bytes_read=%d err!\n", __func__, __LINE__, bytes_read);
                        break;
                    }
                    rt_kprintf("%s %d: bytes_read=%d\n", __func__, __LINE__, bytes_read);
                    if (bytes_read < dl_len)
                    {
                        rt_kprintf("%s %d: network slow or download done\n", __func__, __LINE__);
                    }

                    g_mp3_ring_buffer_write_pos += bytes_read;
                    if (g_mp3_ring_buffer_write_pos >= MP3_RING_BUFFER_SIZE)
                    {
                        g_mp3_ring_buffer_write_pos = 0;
                    }
                    rt_kprintf("%s %d: g_mp3_ring_buffer_write_pos=%d\n", __func__, __LINE__, g_mp3_ring_buffer_write_pos);
                    content_pos += bytes_read;
                    rt_kprintf("%s %d: content_pos=%d\n", __func__, __LINE__, content_pos);
                }
                break;
            case MP3_DL_CMD_STOP:
                rt_kprintf("%s %d: stop\n", __func__, __LINE__);
                goto __exit;
            default:
                break;
        }
    }
    rt_kprintf("%s %d: done\n", __func__, __LINE__);

__exit:

    /* 关闭会话 */
    if (session != RT_NULL)
        webclient_close(session);

    /* free mq */
    rt_mq_delete(g_mp3_dl_mq);
    g_mp3_dl_mq = RT_NULL;

    g_mp3_dl_state = MP3_DL_STATE_IDLE;
    g_mp3_dl_content_len = 0;

    return;
}

int mp3_dl_thread_init(const char *mp3_url)
{
    int ret = 0;
    rt_kprintf("%s %d: g_mp3_dl_state=%d\n", __func__, __LINE__, g_mp3_dl_state);
    if (g_mp3_dl_state == MP3_DL_STATE_IDLE)
    {
#if 0
        g_mp3_dl_mq = rt_mq_create("mp3_dl_mq", sizeof(mp3_ctrl_info_t), 40, RT_IPC_FLAG_FIFO);
        RT_ASSERT(g_mp3_dl_mq);
        g_mp3_dl_thread = rt_thread_create("mp3_dl", mp3_dl_thread_entry, (void *)mp3_url, 2048, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
        RT_ASSERT(g_mp3_dl_thread);
        rt_err_t err = rt_thread_startup(g_mp3_dl_thread);
        RT_ASSERT(RT_EOK == err);
#endif
        g_mp3_dl_content_pos = 0;
        ret = mp3_network_get_part(mp3_url, g_mp3_ring_buffer, MP3_RING_BUFFER_SIZE, mp3_dl_get_part_callback);
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
    user_cb = (mp3_user_cb)parameter;

    rt_kprintf("%s %d: wait dl timeout\n", __func__, __LINE__);
    if (user_cb)
    {
        user_cb(-1);
    }
    rt_timer_stop(g_mp3_dl_start_timer);
}

void mp3_stream_start(const char *mp3_url, void *user_cb)
{
    mp3_dl_thread_init(mp3_url);
    if (g_mp3_dl_state == MP3_DL_STATE_INIT)
    {
        if (!g_mp3_dl_start_timer)
        {
            g_mp3_dl_start_timer = rt_timer_create("mp3start", mp3_stream_start_timer_cb, user_cb,
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
        send_stop_msg_to_mp3_dl();
        play_stop();
    }
}

uint32_t mp3_stream_get_total_seconds(void)
{
    return play_get_total_seconds();
}

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

/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/

