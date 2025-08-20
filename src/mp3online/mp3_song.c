
#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include <webclient.h>
#include <cJSON.h>

#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"
#include "mp3_mem.h"
#include "mp3_network.h"
#include "mp3_song.h"

extern cJSON *mp3_cust_list_get(void);

static int mp3_songs_get_infos(cJSON *json)
{
    cJSON *cust_list = mp3_cust_list_get();
    RT_ASSERT(cust_list);

    cJSON *songs_array = cJSON_GetObjectItem(json, "songs");
    RT_ASSERT(songs_array);

    int count = cJSON_GetArraySize(songs_array);
    for (int i = 0; i < count; i++)
    {
        cJSON *song_item = cJSON_GetArrayItem(songs_array, i);
        RT_ASSERT(song_item);

        cJSON *id = cJSON_GetObjectItem(song_item, "id");
        RT_ASSERT(id);
        /* find id in cust list */
        for (int j = 0; j < cJSON_GetArraySize(cust_list); j++)
        {
            cJSON *cust_item = cJSON_GetArrayItem(cust_list, j);
            RT_ASSERT(cust_item);
            cJSON *cust_item_id = cJSON_GetObjectItem(cust_item, "id");
            if (cJSON_Compare(cust_item_id, id, 0))
            {
                /* add title/artist... to cust_item */
                cJSON *name = cJSON_Duplicate(cJSON_GetObjectItem(song_item, "name"), 1);
                cJSON_AddItemToObject(cust_item, "name", name);
                /* only keep first artist */
                cJSON *artist = cJSON_GetArrayItem(cJSON_GetObjectItem(song_item, "ar"), 0);
                cJSON *artist_name = cJSON_Duplicate(cJSON_GetObjectItem(artist, "name"), 1);
                cJSON_AddItemToObject(cust_item, "artist", artist_name);
            }
        }
    }
    rt_kprintf("%s:%s\n", __func__, cJSON_PrintUnformatted(cust_list));
}

static int mp3_songs_content_callback(uint8_t *data, size_t len)
{
    rt_kprintf("%s: data[%d]=%s\n", __func__, len, data);
    cJSON *json = cJSON_Parse(data);
    RT_ASSERT(json);
    mp3_mem_free(data);

    mp3_songs_get_infos(json);

    cJSON_Delete(json);
}

int mp3_update_songs_info(cJSON *playlist)
{
    int ret = 0;

    cJSON* request = cJSON_CreateObject();
    //cJSON* c = cJSON_AddArrayToObject(request, "c");
    //cJSON* ids = cJSON_AddArrayToObject(request, "ids");
    cJSON* c = cJSON_CreateArray();
    cJSON* ids = cJSON_CreateArray();

    /* format post request data to get first 5 songs */
    int max = cJSON_GetArraySize(playlist);
    //int *id_array = (int *)mp3_mem_malloc(max * sizeof(int));
    //RT_ASSERT(id_array);

    cJSON *song_item = NULL;
    cJSON *id_item = NULL;
    double id_val = 0.0;
    for (int i = 0; i < max; i++)
    {
        song_item = cJSON_GetArrayItem(playlist, i);
        id_item = cJSON_GetObjectItem(song_item, "id");
        if (cJSON_IsNumber(id_item))
        {
            id_val = cJSON_GetNumberValue(id_item);
            cJSON* id_num = cJSON_CreateNumber(id_val);
            cJSON_AddItemToArray(ids, id_num);

            cJSON* c_num = cJSON_CreateNumber(id_val);
            cJSON* c_item = cJSON_CreateObject();
            cJSON_AddItemToObject(c_item, "id", c_num);
            cJSON_AddItemToArray(c, c_item);
        }
    }

    //rt_kprintf("%s\n", cJSON_PrintUnformatted(c));
    cJSON_AddStringToObject(request, "c", cJSON_PrintUnformatted(c));
    rt_kprintf("%s\n", cJSON_PrintUnformatted(ids));
    cJSON_AddStringToObject(request, "ids", cJSON_PrintUnformatted(ids));
    //rt_kprintf("%s\n", cJSON_PrintUnformatted(request));

    char *mp3_url = "https://music.163.com/weapi/v3/song/detail";
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

    /* data for post */
    req_weapi = weapi(request);
    //rt_kprintf("%s: req_weapi: %s\n", __func__, cJSON_PrintUnformatted(req_weapi));

    post_data = cJSON_to_query_string(req_weapi);
    //rt_kprintf("%s: post: %s\n", __func__, post_data);

    //rt_kprintf("%s: send post\n", __func__);
    /* send POST */
    ret = mp3_network_post(mp3_url, post_data, strlen(post_data), mp3_songs_content_callback);
    if (ret < 0)
    {
        rt_kprintf("%s: post fail %d\n", __func__, ret);
    }

    //rt_kprintf("%s: req_weapi=%x\n", __func__, req_weapi);
    if (req_weapi) cJSON_Delete(req_weapi);
    //rt_kprintf("%s: request=%x\n", __func__, request);
    if (request) cJSON_Delete(request);
    rt_kprintf("%s: done\n", __func__);
    return ret;
}
