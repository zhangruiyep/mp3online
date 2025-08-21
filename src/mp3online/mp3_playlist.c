
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
#include "mp3_song.h"

void mp3_playlist_content_handle(const char *content);

static rt_mq_t g_mp3_playlist_mq = NULL;
static rt_thread_t g_mp3_playlist_thread = NULL;

/* save id,title,len... in our list  */
static cJSON *g_mp3_playlist_json = NULL;
cJSON *mp3_cust_list_get(void)
{
    return g_mp3_playlist_json;
}

char *mp3_playlist_get_song_title(int index)
{
    return cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(g_mp3_playlist_json, index), "name"));
}

char *mp3_playlist_get_song_artist(int index)
{
    return cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(g_mp3_playlist_json, index), "artist"));
}

int mp3_playlist_get_count(void)
{
    /* only count tracks which name is updated */
    int count = 0;
    for (int i = 0; i < cJSON_GetArraySize(g_mp3_playlist_json); i++)
    {
        char *name = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(g_mp3_playlist_json, i), "name"));
        if (name && (strlen(name) > 0))
        {
            count++;
        }
    }
    return count;
}

static int mp3_playlist_content_callback(uint8_t *data, size_t len)
{
    cJSON *json = cJSON_Parse(data);
    RT_ASSERT(json);
    mp3_mem_free(data);

    cJSON *trackIds = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "playlist"), "trackIds");
    RT_ASSERT(trackIds);

    /* save in our playlist */
    if (g_mp3_playlist_json)
    {
        cJSON_Delete(g_mp3_playlist_json);
    }
    g_mp3_playlist_json = cJSON_CreateArray();

    for (int i = 0; i < cJSON_GetArraySize(trackIds); i++)
    {
        cJSON *id = cJSON_GetObjectItem(cJSON_GetArrayItem(trackIds, i), "id");
        if (cJSON_IsNumber(id))
        {
            double id_val = cJSON_GetNumberValue(id);
            //rt_kprintf("id[%d]=%f\n", i, id_val);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id_val);
            cJSON_AddItemToArray(g_mp3_playlist_json, item);
        }
    }
    cJSON_Delete(json);

    mp3_update_songs_info(g_mp3_playlist_json);
}

int mp3_playlist_get(const char * playlist_id)
{
    int ret = 0;
    char *mp3_url = "https://music.163.com/weapi/v3/playlist/detail";
    cJSON *req_data = NULL;
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

    /* data for post */
    //req_data = cJSON_Parse("{\"id\":\"2819914042\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");
    //req_data = cJSON_Parse("{\"id\":\"2809577409\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");
    req_data = cJSON_Parse("{\"id\":\"10007604484\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");

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

static void mp3_playlist(int argc, char **argv)
{
    mp3_playlist_get("2819914042");
}
MSH_CMD_EXPORT(mp3_playlist, MP3 playlist test)

