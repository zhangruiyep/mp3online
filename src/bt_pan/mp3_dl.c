
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/dns.h"
#include <webclient.h>
#include <cJSON.h>
#include "local_music.h"

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#define GET_HEADER_BUFSZ        2048        //头部大小
#define GET_RESP_BUFSZ          8192        //响应缓冲区大小
#define GET_URL_LEN_MAX         256         //网址最大长度
#define MP3_HOST_NAME           "music.163.com"
//#define MP3_HOST_BASE_URL       "https://music.taihe.com/v1"
//#define MP3_PLAYLIST_API        "/tracklist/info"

extern uint8_t g_mp3_ring_buffer[1024*16];
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

static uint8_t is_ip_searching;

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

void svr_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL)
    {
        rt_kprintf("DNS lookup succeeded, IP: %s\n", ipaddr_ntoa(ipaddr));
    }
}

int check_internet_access()
{
    int r = 0;
    const char *hostname = MP3_HOST_NAME;
    ip_addr_t addr = {0};

    {
        err_t err = dns_gethostbyname(hostname, &addr, svr_found_callback, NULL);
        if (err != ERR_OK && err != ERR_INPROGRESS)
        {
            rt_kprintf("Coud not find %s, please check PAN connection\n", hostname);
        }
        else
            r = 1;
    }

    return r;
}

int mp3_dl(void)
{
    char *buffer = RT_NULL;
    int resp_status;
    struct webclient_session *session = RT_NULL;
    char *playlist_url = RT_NULL;
    int content_length = -1, bytes_read = 0;
    int content_pos = 0;

    while (check_internet_access() == 0)
    {
        rt_kprintf("no internet, wait...\n");
        rt_thread_mdelay(2000);
    }

    /* 为 playlist_url 分配空间 */
    playlist_url = rt_calloc(1, GET_URL_LEN_MAX);
    if (playlist_url == RT_NULL)
    {
        rt_kprintf("No memory for playlist_url!\n");
        goto __exit;
    }

    /* 拼接 GET 网址 */
    rt_snprintf(playlist_url, GET_URL_LEN_MAX, "http://music.163.com/song/media/outer/url?id=2639639291.mp3");

    /* 创建会话并且设置响应的大小 */
    session = webclient_session_create(GET_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        rt_kprintf("No memory for get header!\n");
        goto __exit;
    }

    /* 发送 GET 请求使用默认的头部 */
    if ((resp_status = webclient_get(session, playlist_url)) != 200)
    {
        rt_kprintf("webclient GET request failed, response(%d) error.\n", resp_status);
        goto __exit;
    }

#if 0
    /* 分配用于存放接收数据的缓冲 */
    buffer = rt_calloc(1, GET_RESP_BUFSZ);
    if (buffer == RT_NULL)
    {
        rt_kprintf("No memory for data receive buffer!\n");
        goto __exit;
    }
#endif

    content_length = webclient_content_length_get(session);
    if (content_length > 0)
    {
        rt_kprintf("content_length==%d\n", content_length);
        g_mp3_dl_content_len = content_length;

        bytes_read = webclient_read(session, g_mp3_ring_buffer, MIN(sizeof(g_mp3_ring_buffer), content_length));
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
                    remain_len += sizeof(g_mp3_ring_buffer);
                }
                rt_kprintf("%s %d: remain_len=%d\n", __func__, __LINE__, remain_len);

                if (remain_len < sizeof(g_mp3_ring_buffer)/2)
                {
                    /* make sure dl write in buffer range */
                    rt_kprintf("%s %d: g_mp3_ring_buffer_write_pos=%d\n", __func__, __LINE__, g_mp3_ring_buffer_write_pos);
                    int dl_len = sizeof(g_mp3_ring_buffer) - g_mp3_ring_buffer_write_pos;
                    if (dl_len > sizeof(g_mp3_ring_buffer)/2)
                    {
                        dl_len = sizeof(g_mp3_ring_buffer)/2;
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
                    if (g_mp3_ring_buffer_write_pos >= sizeof(g_mp3_ring_buffer))
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
    if (playlist_url != RT_NULL)
    {
        rt_free(playlist_url);
        playlist_url = RT_NULL;
    }

    /* 关闭会话 */
    if (session != RT_NULL)
        webclient_close(session);

    //return buffer;
    return content_length;
}

void mp3_dl_thread_entry(void *params)
{
    mp3_dl();
}

int mp3_dl_thread_init(void)
{
    g_mp3_dl_mq = rt_mq_create("mp3_dl_mq", sizeof(mp3_ctrl_info_t), 60, RT_IPC_FLAG_FIFO);
    RT_ASSERT(g_mp3_dl_mq);
    g_mp3_dl_thread = rt_thread_create("mp3_dl", mp3_dl_thread_entry, NULL, 2048, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
    RT_ASSERT(g_mp3_dl_thread);
    rt_err_t err = rt_thread_startup(g_mp3_dl_thread);
    RT_ASSERT(RT_EOK == err);
}

__ROM_USED void mp3play(int argc, char **argv)
{
#if 0
    //char *mp3_buff = mp3_dl();
    int len = mp3_dl();
    if (len)
    {
        play_buff(g_mp3_ring_buffer, len);
    }


    if (mp3_buff)
    {
        rt_kprintf("mp3_buff:%s\n", mp3_buff);
        play_buff(mp3_buff, GET_RESP_BUFSZ);
        rt_free(mp3_buff);
    }
#endif
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

