
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

static void mp3_ne_url_test(int argc, char **argv)
{
    if (strcmp(argv[1], "cookie") == 0)
    {
        ne_init_cookie();
        rt_kprintf("cookie: %s", ne_get_cookie());
    }
}
MSH_CMD_EXPORT(mp3_ne_url_test, MP3 NE URL api test);
