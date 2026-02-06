

#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <cJSON.h>
#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"
#include "mp3_mem.h"
#include "mp3_network.h"
#ifdef RT_USING_DFS
#include <dfs_posix.h>
#endif

extern cJSON *mp3_cust_list_get(void);
extern void mp3_playlist_get_song_id(int index, char *id);

static char cur_track_id[32] = {0};

#ifdef RT_USING_DFS
static int mp3_lyric_save(const char *track_id, cJSON *lyric)
{
    char filename[32] = {0};
    snprintf(filename, sizeof(filename), "/lyric_%s.json", track_id);

    char *json_str = cJSON_Print(lyric);
    if (json_str == NULL)
    {
        return -1;
    }

    /* check file exist */
    struct stat st = {0};
    int ret = stat(filename, &st);
    if (ret == 0)
    {
        if ((st.st_size > 0) && (strlen(json_str) == st.st_size))
        {
            rt_kprintf("%s: file %s already exist, skip write\n", __func__, filename);
            return 0;
        }
    }

    int fd;
    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0);
    if (fd >= 0)
    {
        write(fd, json_str, strlen(json_str));
        close(fd);
        rt_kprintf("%s: write %s %d bytes OK\n", __func__, filename, strlen(json_str));
        ret = 0;
    }
    else
    {
        rt_kprintf("%s: open file %s failed!\n", __func__, filename);
        ret = -1;
    }

    cJSON_free(json_str);
    return ret;
}
#endif

static int mp3_lyric_get_data(cJSON *json)
{
    cJSON *cust_list = mp3_cust_list_get();
    RT_ASSERT(cust_list);

    cJSON *lrc_lyric = cJSON_GetObjectItem(cJSON_GetObjectItem(json, "lrc"), "lyric");
    RT_ASSERT(lrc_lyric);

#ifdef RT_USING_DFS
    /* save to flash */
    mp3_lyric_save(cur_track_id, lrc_lyric);
#endif

    //rt_kprintf("%s: lyric=%s\n", __func__, cJSON_GetStringValue(lrc_lyric));

    /* find id in cust list */
    for (int j = 0; j < cJSON_GetArraySize(cust_list); j++)
    {
        char cust_id[32] = {0};
        mp3_playlist_get_song_id(j, cust_id);
        //rt_kprintf("%s: cust_id %s\n", __func__, cust_id);
    
        cJSON *cust_item = cJSON_GetArrayItem(cust_list, j);
        RT_ASSERT(cust_item);
        if (strcmp(cust_id, cur_track_id) == 0)
        {
            /* add lyric to cust_item */
            rt_kprintf("%s: add lyric for id %s\n", __func__, cur_track_id);
            cJSON *lyric = cJSON_Duplicate(lrc_lyric, 1);
            cJSON_AddItemToObject(cust_item, "lyric", lyric);
        }
    }
    return 0;
}

static int mp3_lyric_content_callback(uint8_t *data, size_t len)
{
    //rt_kprintf("%s: data[%d]=%s\n", __func__, len, data);
    cJSON *json = cJSON_Parse(data);
    RT_ASSERT(json);
    mp3_mem_free(data);

    mp3_lyric_get_data(json);

    cJSON_Delete(json);
    return 0;
}

int mp3_lyric_get(const char * track_id)
{
    int ret = 0;
    char *mp3_url = "https://music.163.com/weapi/song/lyric?csrf_token=";
    cJSON *req_data = NULL;
    cJSON *req_weapi = NULL;
    char *post_data = NULL;

    strcpy(cur_track_id, track_id);
    //rt_kprintf("%s: cur_track_id=%s\n", __func__, cur_track_id);

    /* data for post */
    req_data = cJSON_CreateObject();
    cJSON_AddStringToObject(req_data, "id", track_id);
    cJSON_AddNumberToObject(req_data, "lv", -1);
    cJSON_AddNumberToObject(req_data, "tv", -1);
    cJSON_AddStringToObject(req_data, "csrf_token", "");

    req_weapi = weapi(req_data);
    if (req_data) cJSON_Delete(req_data);

    post_data = cJSON_to_query_string(req_weapi);
    if (req_weapi) cJSON_Delete(req_weapi);

    /* send POST */
    ret = mp3_network_post(mp3_url, post_data, strlen(post_data), mp3_lyric_content_callback);
    if (ret < 0)
    {
        rt_kprintf("%s: post fail %d\n", __func__, ret);
    }
    //if (post_data) mp3_mem_free(post_data);
    return ret;
}

int mp3_lyric_get_by_time(int index, uint32_t secs, char *lyric_line)
{
    static char *p = NULL;
    static char *last_lyric = NULL;
    bool lyric_refreshed = false;
    
    if (secs <= 1)
    {
        p = NULL;
        last_lyric = NULL;
        return 0;
    }

    if (p == NULL)
    {
        cJSON *cust_list = mp3_cust_list_get();
        RT_ASSERT(cust_list);
        char *lyric = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(cust_list, index), "lyric"));
        //RT_ASSERT(lyric);
        if (lyric == NULL)
        {
            rt_kprintf("%s: index=%d NO LYRIC\n", __func__, index);
            return -1;
        }

        p = lyric;
    }

    rt_kprintf("%s: index=%d, secs=%d\n", __func__, index, secs);
    /* find lyric line by time */
    while (p && (*p != '\0'))
    {
        char *time_s = strstr(p, "[");
        //rt_kprintf("%s: [ at %x\n", __func__, time_s);
        if (time_s)
        {
            char *time_e = strstr(time_s, "]");
            //rt_kprintf("%s: ] at %x\n", __func__, time_e);
            if (time_e)
            {
                int min, sec, ms = 0;
                sscanf(time_s, "[%d:%d.%d]", &min, &sec, &ms);
                uint32_t time = min * 60 + sec;
                //rt_kprintf("%s: time=%d\n", __func__, time);
                if (time <= secs)
                {
                    char *lyric_e = strstr(time_e, "\n");
                    //rt_kprintf("%s: newline at %x\n", __func__, lyric_e);
                    strncpy(lyric_line, time_e + 1, lyric_e - (time_e + 1));
                    lyric_refreshed = true;
                    last_lyric = time_e + 1;
                    p = lyric_e + 1;
                }
                else
                {
                    break;
                }
            }
        }
        
    }

    if ((!lyric_refreshed) && last_lyric)
    {
        char *lyric_e = strstr(last_lyric, "\n");
        strncpy(lyric_line, last_lyric, lyric_e - last_lyric);
    }

    return 0;
}

