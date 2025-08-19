
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
#include "mp3_mem.h"
#include "mp3_network.h"

void mp3_playlist_content_handle(const char *content);

static rt_mq_t g_mp3_playlist_mq = NULL;
static rt_thread_t g_mp3_playlist_thread = NULL;

static int mp3_playlist_content_callback(uint8_t *data, size_t len)
{
    cJSON *json = cJSON_Parse(data);
    RT_ASSERT(json);
    mp3_mem_free(data);

    cJSON *trackIds = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "playlist"), "trackIds");
    RT_ASSERT(trackIds);

    cJSON_Delete(json);
}

//void mp3_playlist_thread_entry(void *params)
int mp3_playlist_get(int playlist_id)
{
    int ret = 0;
    char *mp3_url = "https://music.163.com/weapi/v3/playlist/detail";
    cJSON *req_data = NULL;
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

    /* data for post */
    req_data = cJSON_Parse("{\"id\":\"2819914042\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");

    req_weapi = weapi(req_data);
    if (req_data) cJSON_Delete(req_data);

    post_data = cJSON_to_query_string(req_weapi);
    if (req_weapi) cJSON_Delete(req_weapi);

    /* send POST */
    ret = mp3_network_post(mp3_url, post_data, strlen(post_data), mp3_playlist_content_callback);
    if (ret < 0)
    {
        rt_kprintf("%s: post fail %d\n", __func__, ret);
    }
    //if (post_data) mp3_mem_free(post_data);
    return ret;
}

#if 0
int mp3_playlist_thread_init(void)
{
    //g_mp3_playlist_mq = rt_mq_create("mp3_playlist_mq", sizeof(mp3_ctrl_info_t), 60, RT_IPC_FLAG_FIFO);
    //RT_ASSERT(g_mp3_playlist_mq);
    g_mp3_playlist_thread = rt_thread_create("mp3plist", mp3_playlist_thread_entry, NULL, 4096, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
    RT_ASSERT(g_mp3_playlist_thread);
    rt_err_t err = rt_thread_startup(g_mp3_playlist_thread);
    RT_ASSERT(RT_EOK == err);
}

void mp3_playlist_content_handle(const char *content)
{
    cJSON *json = cJSON_Parse(content);
    RT_ASSERT(json);

    cJSON *trackIds = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "playlist"), "trackIds");
    RT_ASSERT(trackIds);
#if 0
    for (int i = 0; i < cJSON_GetArraySize(trackIds); i++)
    {
        cJSON *trackId = cJSON_GetArrayItem(trackIds, i);
        rt_kprintf("trackId[%d]=%s\n", i, cJSON_PrintUnformatted(trackId));
        cJSON *id = cJSON_GetObjectItem(trackId, "id");
        if (cJSON_IsNumber(id))
        {
            double id_val = cJSON_GetNumberValue(id);
            rt_kprintf("id[%d]=%f\n", i, id_val);
        }
    }
#endif
    cJSON_Delete(json);
}
#endif

static void mp3_playlist(int argc, char **argv)
{
    //mp3_playlist_thread_init();
    mp3_playlist_get(2819914042);
}
MSH_CMD_EXPORT(mp3_playlist, MP3 playlist test)

