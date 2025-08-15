
/* js url functions */

#include <rtthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <cJSON.h>

#include "mp3_ne_sec.h"
#include "mp3_ne_url.h"

static char *g_cookie = NULL;
void ne_init_cookie(void)
{
    if (g_cookie == NULL)
    {
        g_cookie = (char *)malloc(256);
    }
    RT_ASSERT(g_cookie);

    uint8_t *nuidValue = ne_create_secret_key(32);
    char nnidValue[64] = {0};
    size_t now = time(NULL);
    sprintf(nnidValue, "%s,%d", nuidValue, now);

    sprintf(g_cookie, "_ntes_nuid=%s; _ntes_nnid=%s", nuidValue, nnidValue);
    return;
}

char *ne_get_cookie(void)
{
    return g_cookie;
}

void ne_set_cookie(char *cookie)
{
    if (g_cookie)
    {
        strcpy(g_cookie, cookie);
    }
}

char *cJSON_to_query_string(cJSON *json)
{
    RT_ASSERT(json);
    char *query_string = (char *)rt_malloc(2048);
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

static void mp3_ne_url_test(int argc, char **argv)
{
    if (strcmp(argv[1], "cookie") == 0)
    {
        ne_init_cookie();
        rt_kprintf("cookie: %s", ne_get_cookie());
    }
}
MSH_CMD_EXPORT(mp3_ne_url_test, MP3 NE URL api test);
