
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/dns.h"
#include <webclient.h>
#include <cJSON.h>
#if PKG_NETUTILS_NTP
#include "ntp.h"
#endif
#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#define POST_URL_LEN_MAX               256
#define POST_RESP_BUFSZ                1024
#define POST_HEADER_BUFSZ              1024

extern int check_internet_access(void);

static rt_mq_t g_mp3_playlist_mq = NULL;
static rt_thread_t g_mp3_playlist_thread = NULL;

void mp3_playlist_thread_entry(void *params)
{
    struct webclient_session* session = RT_NULL;
    unsigned char *buffer = RT_NULL;
    int index, ret = 0;
    int bytes_read, resp_status;
    char *mp3_url = NULL;
    cJSON *req_data = NULL;
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

    while (check_internet_access() == 0)
    {
        rt_kprintf("no internet, wait...\n");
        rt_thread_mdelay(2000);
    }

#if PKG_NETUTILS_NTP
    /* sync time before download */
    ntp_sync_to_rtc(RT_NULL);
#endif

    buffer = (unsigned char *) web_malloc(POST_RESP_BUFSZ);
    if (buffer == RT_NULL)
    {
        rt_kprintf("no memory for receive response buffer.\n");
        ret = -RT_ENOMEM;
        goto __exit;
    }

    /* create webclient session and set header response size */
    session = webclient_session_create(POST_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        ret = -RT_ENOMEM;
        goto __exit;
    }

    mp3_url = (char *)web_malloc(POST_URL_LEN_MAX);
    if (mp3_url == RT_NULL)
    {
        rt_kprintf("No memory for mp3_url!\n");
        goto __exit;
    }
    rt_snprintf(mp3_url, POST_URL_LEN_MAX, "https://music.163.com/weapi/v3/playlist/detail");

    /* data for post */
    req_data = cJSON_Parse("{\"id\":\"2819914042\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");
    req_weapi = weapi(req_data);
    post_data = cJSON_to_query_string(req_weapi);

    /* build header for upload */
    ne_init_cookie();
    webclient_header_fields_add(session, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:142.0) Gecko/20100101 Firefox/142.0\r\n");
    webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/x-www-form-urlencoded;charset=utf-8\r\n");
    webclient_header_fields_add(session, "Cookie: %s\r\n", ne_get_cookie());

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, mp3_url, post_data, strlen(post_data))) != 200)
    {
        rt_kprintf("webclient POST request failed, response(%d) error.\n", resp_status);
        ret = -RT_ERROR;
        goto __exit;
    }

    int content_length = webclient_content_length_get(session);
    if (content_length == 0)
    {
        rt_kprintf("webclient post response data is null.\n");
        ret = -RT_ERROR;
        goto __exit;
    }

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

__exit:
    if (session)
    {
        webclient_close(session);
        session = RT_NULL;
    }

    if (buffer)
    {
        web_free(buffer);
    }

    if (req_data) cJSON_Delete(req_data);
    if (req_weapi) cJSON_Delete(req_weapi);
    if (post_data) rt_free(post_data);
    if (mp3_url) web_free(mp3_url);

    return;
}

int mp3_playlist_thread_init(void)
{
    //g_mp3_playlist_mq = rt_mq_create("mp3_playlist_mq", sizeof(mp3_ctrl_info_t), 60, RT_IPC_FLAG_FIFO);
    //RT_ASSERT(g_mp3_playlist_mq);
    g_mp3_playlist_thread = rt_thread_create("mp3plist", mp3_playlist_thread_entry, NULL, 4096, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
    RT_ASSERT(g_mp3_playlist_thread);
    rt_err_t err = rt_thread_startup(g_mp3_playlist_thread);
    RT_ASSERT(RT_EOK == err);
}

static void mp3_playlist(int argc, char **argv)
{
    mp3_playlist_thread_init();
}
MSH_CMD_EXPORT(mp3_playlist, MP3 playlist test)