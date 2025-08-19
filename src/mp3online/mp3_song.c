
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include <webclient.h>
#include <cJSON.h>

#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"
#include "mp3_mem.h"
#include "mp3_network.h"

static int mp3_songs_content_callback(uint8_t *data, size_t len)
{
    cJSON *json = cJSON_Parse(data);
    RT_ASSERT(json);
    mp3_mem_free(data);

    //cJSON *trackIds = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "playlist"), "trackIds");
    //RT_ASSERT(trackIds);

    cJSON_Delete(json);
}

#if 0
int mp3_get_songs_info(cJSON *request)
{
    char *mp3_url = NULL;
    cJSON *req_data = NULL;
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

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
    rt_snprintf(mp3_url, POST_URL_LEN_MAX, "https://music.163.com/weapi/v3/song/detail");

    /* data for post */
    req_data = cJSON_Parse("{\"id\":\"2819914042\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");
    req_weapi = weapi(req_data);
    post_data = cJSON_to_query_string(req_weapi);

    /* build header for upload */
    ne_init_cookie();
    char *cookie_str = ne_get_cookie();
    RT_ASSERT(cookie_str);

    webclient_header_fields_add(session, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:142.0) Gecko/20100101 Firefox/142.0\r\n");
    webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
    webclient_header_fields_add(session, "Content-Type: application/x-www-form-urlencoded;charset=utf-8\r\n");
    webclient_header_fields_add(session, "Cookie: %s\r\n", cookie_str);

    free(cookie_str);

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, mp3_url, post_data, strlen(post_data))) != 200)
    {
        rt_kprintf("webclient POST request failed, response(%d) error.\n", resp_status);
        ret = -RT_ERROR;
        goto __exit;
    }

    /* handle set cookie */
    const char *set_cookie = webclient_header_fields_get(session, "Set-Cookie:");
    if (set_cookie)
    {
        char *nmtid = strstr(set_cookie, "NMTID=");
        if (nmtid)
        {
            //rt_kprintf("%s: found NMTID=%s\n", __func__, nmtid);
            char nmtid_value[64] = {0};
            if (sscanf(nmtid, "NMTID=%[^;]", nmtid_value) > 0)
            {
                //rt_kprintf("%s: nmtid_value=%s\n", __func__, nmtid_value);
                ne_set_cookie_item("NMTID", nmtid_value);
            }
        }
    }
    else
    {
        //rt_kprintf("Set-Cookie: not found\n");
    }

    int content_length = webclient_content_length_get(session);
    if (content_length == 0)
    {
        rt_kprintf("webclient post response data is null.\n");
        ret = -RT_ERROR;
        goto __exit;
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
    char *content = mp3_mem_malloc(content_length + 1);
    RT_ASSERT(content);
    memset(content, 0, content_length + 1);
    bytes_read = webclient_read(session, content, content_length);
    RT_ASSERT(bytes_read == content_length);

    mp3_playlist_content_handle(content);
    if (content) mp3_mem_free(content);

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
#endif

int mp3_song_trackIds_handle(cJSON *trackIds)
{
    int ret = 0;

    cJSON* request = cJSON_CreateObject();
    cJSON* c = cJSON_AddArrayToObject(request, "c");
    cJSON* ids = cJSON_AddArrayToObject(request, "ids");
    double id_val = 0.0;

    /* format post request data to get first 5 songs */
    int max = MIN(cJSON_GetArraySize(trackIds), 5);
    //int *id_array = (int *)mp3_mem_malloc(max * sizeof(int));
    //RT_ASSERT(id_array);
    for (int i = 0; i < max; i++)
    {
        cJSON *trackId = cJSON_GetArrayItem(trackIds, i);
        cJSON *id = cJSON_GetObjectItem(trackId, "id");
        if (cJSON_IsNumber(id))
        {
            id_val = cJSON_GetNumberValue(id);
            //id_array[i] = (int)id_val;
            cJSON* id_num = cJSON_CreateNumber(id_val);
            cJSON_AddItemToArray(ids, id_num);

            cJSON* c_num = cJSON_CreateNumber(id_val);
            cJSON* c_item = cJSON_CreateObject();
            cJSON_AddItemToObject(c_item, "id", c_num);
            cJSON_AddItemToArray(c, c_item);
        }
    }
    //cJSON* ids = cJSON_CreateIntArray(id_array, max);
    //cJSON_AddItemToObject(request, "ids", ids);

    rt_kprintf("%s\n", cJSON_PrintUnformatted(c));
    rt_kprintf("%s\n", cJSON_PrintUnformatted(ids));
    rt_kprintf("%s\n", cJSON_PrintUnformatted(request));

    char *mp3_url = "https://music.163.com/weapi/v3/song/detail";
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

    /* data for post */
    req_weapi = weapi(request);
    if (request) cJSON_Delete(request);

    post_data = cJSON_to_query_string(req_weapi);
    if (req_weapi) cJSON_Delete(req_weapi);

    /* send POST */
    ret = mp3_network_post(mp3_url, post_data, strlen(post_data), mp3_songs_content_callback);
    if (ret < 0)
    {
        rt_kprintf("%s: post fail %d\n", __func__, ret);
    }

    return ret;
}
