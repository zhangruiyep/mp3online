
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/dns.h"
#include <webclient.h>
#include <cJSON.h>
#if PKG_NETUTILS_NTP
#include "ntp.h"
#endif
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

typedef enum
{
    MP3_DL_CMD_READ_MORE,
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

static rt_mq_t g_mp3_dl_mq = NULL;
static rt_thread_t g_mp3_dl_thread = NULL;

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

void mp3_dl_thread_entry(void *params)
{
    char *buffer = RT_NULL;
    int resp_status;
    struct webclient_session *session = RT_NULL;
    char *mp3_url = RT_NULL;
    int content_length = -1, bytes_read = 0;
    int content_pos = 0;

    /* 为 mp3_url 分配空间 */
    mp3_url = rt_calloc(1, GET_URL_LEN_MAX);
    if (mp3_url == RT_NULL)
    {
        rt_kprintf("No memory for mp3_url!\n");
        goto __exit;
    }

    /* 拼接 GET 网址 */
    rt_snprintf(mp3_url, GET_URL_LEN_MAX, "http://music.163.com/song/media/outer/url?id=2155423468.mp3");

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
        goto __exit;
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
                        rt_kprintf("%s bytes_read=%d err!!\n", bytes_read);
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
            default:
                break;
        }
    }
    rt_kprintf("%s %d: done\n", __func__, __LINE__);

__exit:
    /* 释放网址空间 */
    if (mp3_url != RT_NULL)
    {
        rt_free(mp3_url);
        mp3_url = RT_NULL;
    }

    /* 关闭会话 */
    if (session != RT_NULL)
        webclient_close(session);

    /* free mq */
    rt_mq_delete(g_mp3_dl_mq);
    g_mp3_dl_mq = RT_NULL;

    g_mp3_dl_state = MP3_DL_STATE_IDLE;

    return;
}

int mp3_dl_thread_init(void)
{
    if (g_mp3_dl_state == MP3_DL_STATE_IDLE)
    {
        g_mp3_dl_mq = rt_mq_create("mp3_dl_mq", sizeof(mp3_ctrl_info_t), 10, RT_IPC_FLAG_FIFO);
        RT_ASSERT(g_mp3_dl_mq);
        g_mp3_dl_thread = rt_thread_create("mp3_dl", mp3_dl_thread_entry, NULL, 2048, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
        RT_ASSERT(g_mp3_dl_thread);
        rt_err_t err = rt_thread_startup(g_mp3_dl_thread);
        RT_ASSERT(RT_EOK == err);
        g_mp3_dl_state = MP3_DL_STATE_INIT;
    }
}

void mp3_stream_resume(void)
{
    mp3_dl_thread_init();
    if (g_mp3_dl_state = MP3_DL_STATE_INIT)
    {
        int retry = 30;
        while (retry-- > 0)
        {
            rt_kprintf("%s %d: wait dl, retry=%d\n", __func__, __LINE__, retry);
            if (g_mp3_dl_content_len)
            {
                play_buff(g_mp3_ring_buffer, g_mp3_dl_content_len);
                break;
            }
            rt_thread_mdelay(1000);
        }
    }
    else if (g_mp3_dl_state = MP3_DL_STATE_DLING)
    {
        play_resume();
    }
}

void mp3_stream_pause(void)
{
    if (g_mp3_dl_state = MP3_DL_STATE_DLING)
    {
        play_pause();
    }
}

static void mp3play(int argc, char **argv)
{
    mp3_dl_thread_init();
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

