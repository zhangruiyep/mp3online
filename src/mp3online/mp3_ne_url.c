
/* js url functions */

#include <rtthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <cJSON.h>

#include "mp3_mem.h"
#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"

static char *g_cookie = NULL;
static cJSON *g_cookie_json = NULL;

void ne_init_cookie(void)
{
    if (g_cookie_json == NULL)
    {
        g_cookie_json = cJSON_CreateObject();
        RT_ASSERT(g_cookie_json);

        uint8_t *nuidValue = ne_create_secret_key(32);
        char nnidValue[64] = {0};
        size_t now = time(NULL);
        sprintf(nnidValue, "%s,%d", nuidValue, now);

        cJSON_AddStringToObject(g_cookie_json, "_ntes_nuid", nuidValue);
        cJSON_AddStringToObject(g_cookie_json, "_ntes_nnid", nnidValue);
    }
    return;
}

char *ne_get_cookie(void)
{
    return cJSON_to_cookie_string(g_cookie_json);
}

static void ne_set_cookie_item(const char *name, const char *value)
{
    //rt_kprintf("%s: %s=%s", __func__, name, value);
    if (g_cookie_json)
    {
        cJSON_AddStringToObject(g_cookie_json, name, value);
    }
}

void ne_set_cookie(const char *set_cookie)
{
    if (!set_cookie)
    {
        return;
    }

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

char *cJSON_to_query_string(cJSON *json)
{
    RT_ASSERT(json);
    char *query_string = (char *)mp3_mem_malloc(2048);
    RT_ASSERT(query_string);
    memset(query_string, 0, 2048);

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, json)
    {
        if (!cJSON_IsString(item))
        {
            continue;
        }

        sprintf(query_string, "%s%s=", query_string, item->string);
        /* handle special char */
        char *src = cJSON_GetStringValue(item);
        char *dst = query_string + strlen(query_string);
        while (*src)
        {
            switch (*src)
            {
            case '&':
                *dst++ = '%';
                *dst++ = '2';
                *dst++ = '6';
                break;
            case '=':
                *dst++ = '%';
                *dst++ = '3';
                *dst++ = 'D';
                break;
            case ' ':
                *dst++ = '%';
                *dst++ = '2';
                *dst++ = '0';
                break;
            case '+':
                *dst++ = '%';
                *dst++ = '2';
                *dst++ = 'B';
                break;
            case ',':
                *dst++ = '%';
                *dst++ = '2';
                *dst++ = 'C';
                break;
            case '/':
                *dst++ = '%';
                *dst++ = '2';
                *dst++ = 'F';
                break;
            default:
                *dst++ = *src;
                break;
            }
            src++;
        }
        *dst++ = '&';
    }
    int len = strlen(query_string);
    /*  remove last '&' */
    query_string[len - 1] = '\0';
    return query_string;
}

char *cJSON_to_cookie_string(cJSON *json)
{
    RT_ASSERT(json);
    char *cookie_string = (char *)mp3_mem_malloc(2048);
    RT_ASSERT(cookie_string);
    memset(cookie_string, 0, 2048);

    cJSON *item = NULL;
    bool first = true;
    cJSON_ArrayForEach(item, json)
    {
        if (!cJSON_IsString(item))
        {
            continue;
        }
        if (!first)
        {
            sprintf(cookie_string, "%s; ", cookie_string);
        }
        sprintf(cookie_string, "%s%s=%s", cookie_string, item->string, cJSON_GetStringValue(item));
        first = false;
    }
    return cookie_string;
}

static void mp3_ne_url_test(int argc, char **argv)
{
    if (strcmp(argv[1], "cookie") == 0)
    {
        ne_init_cookie();
        rt_kprintf("cookie: %s", ne_get_cookie());
    }
}
MSH_CMD_EXPORT(mp3_ne_url_test, MP3 NE URL api test);
