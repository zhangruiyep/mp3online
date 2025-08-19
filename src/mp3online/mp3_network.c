
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include <webclient.h>
#include <cJSON.h>
#if PKG_NETUTILS_NTP
#include "ntp.h"
#endif
#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"
#include "mp3_mem.h"
#include "mp3_network.h"

extern int check_internet_access(void);
static rt_mq_t g_mp3_network_mq = NULL;
static rt_thread_t g_mp3_network_thread = NULL;

int mp3_network_post(const char *url, const uint8_t *post_data, size_t post_data_len, mp3_nw_rsp_data_callback callback)
{
    mp3_nw_msg_t msg = {MP3_NW_CMD_POST, (char *)url, post_data, post_data_len, callback};
    return rt_mq_send(g_mp3_network_mq, &msg, sizeof(msg));
}

void mp3_network_thread_entry(void *params)
{
    struct webclient_session* session = RT_NULL;
    int ret = 0;
    int bytes_read, resp_status;

    while (check_internet_access() == 0)
    {
        rt_kprintf("no internet, wait...\n");
        rt_thread_mdelay(2000);
    }

#if PKG_NETUTILS_NTP
    /* sync time before download */
    ntp_sync_to_rtc(RT_NULL);
#endif

    while (1)
    {
        mp3_nw_msg_t msg;
        rt_err_t err = rt_mq_recv(g_mp3_network_mq, &msg, sizeof(mp3_nw_msg_t), RT_WAITING_FOREVER);
        RT_ASSERT(RT_EOK == err);

        switch (msg.cmd)
        {
            case MP3_NW_CMD_POST:
            {
                /* create webclient session and set header response size */
                session = webclient_session_create(POST_HEADER_BUFSZ);
                RT_ASSERT(session);

                /* build header for upload */
                ne_init_cookie();
                char *cookie_str = ne_get_cookie();
                RT_ASSERT(cookie_str);

                webclient_header_fields_add(session, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:142.0) Gecko/20100101 Firefox/142.0\r\n");
                webclient_header_fields_add(session, "Content-Length: %d\r\n", msg.post_data_len);
                webclient_header_fields_add(session, "Content-Type: application/x-www-form-urlencoded;charset=utf-8\r\n");
                webclient_header_fields_add(session, "Cookie: %s\r\n", cookie_str);
                mp3_mem_free(cookie_str);

                /* send POST request */
                if ((resp_status = webclient_post(session, msg.url, msg.post_data, msg.post_data_len)) != 200)
                {
                    rt_kprintf("webclient POST request failed, response(%d) error.\n", resp_status);
                    mp3_mem_free((void *)msg.post_data);
                    webclient_close(session);
                    break;
                }

                mp3_mem_free((void *)msg.post_data);

                /* handle set cookie */
                const char *set_cookie = webclient_header_fields_get(session, "Set-Cookie:");
                if (set_cookie)
                {
                    ne_set_cookie(set_cookie);
                }

                int content_length = webclient_content_length_get(session);
                if (content_length == 0)
                {
                    rt_kprintf("webclient post response data is null.\n");
                    webclient_close(session);
                    break;
                }

#if 0   //debug print
                rt_kprintf("webclient post response data: \n");
                do
                {
                    bytes_read = webclient_read(session, buffer, POST_RESP_BUFSZ);
                    if (bytes_read <= 0)
                    {
                        break;
                    }

                    for (index = 0; index < bytes_read; index++)
                    {
                        rt_kprintf("%c", buffer[index]);
                    }
                } while (1);

                rt_kprintf("\n");
#endif
                if (msg.callback)
                {
                    char *content = mp3_mem_malloc(content_length + 1);
                    RT_ASSERT(content);
                    memset(content, 0, content_length + 1);
                    bytes_read = webclient_read(session, content, content_length);
                    RT_ASSERT(bytes_read == content_length);

                    msg.callback(content, bytes_read);
                }

                if (session)
                {
                    webclient_close(session);
                    session = RT_NULL;
                }
                break;
            }
            default:
                break;
        }
    }
    return;
}

int mp3_network_thread_init(void)
{
    g_mp3_network_mq = rt_mq_create("mp3_nw_mq", sizeof(mp3_nw_msg_t), 5, RT_IPC_FLAG_FIFO);
    RT_ASSERT(g_mp3_network_mq);
    g_mp3_network_thread = rt_thread_create("mp3_net", mp3_network_thread_entry, NULL, 4096, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
    RT_ASSERT(g_mp3_network_thread);
    rt_err_t err = rt_thread_startup(g_mp3_network_thread);
    RT_ASSERT(RT_EOK == err);
}
INIT_PRE_APP_EXPORT(mp3_network_thread_init);
