
#include <rtthread.h>
#include <string.h>
#include <webclient.h>
#include <lvgl.h>
#ifdef RT_USING_DFS
#include <dfs_posix.h>
#endif
#include "mp3_mem.h"
#include "mp3_network.h"
#include "mp3_jpg.h"

#define GET_HEADER_BUFSZ        2048        //头部大小
#define GET_RESP_BUFSZ          8192        //响应缓冲区大小
#define GET_URL_LEN_MAX         256         //网址最大长度

#define JPG_URL "http://sample-files.com/downloads/images/jpg/thumbnail_150x150_10.5kb.jpg"
#define JPG_FILE "/mp3_temp.jpg"

extern int check_internet_access(void);

static char g_dl_filename[128] = {0};
static bool g_jpg_file_downloading = false;
static mp3_dl_img_user_callback g_user_callback = NULL;

bool mp3_img_is_downloading(void)
{
    return g_jpg_file_downloading;
}

bool mp3_img_file_is_ready(const char *filename)
{
    /* check if file is downloading */
    if (mp3_img_is_downloading() && (strcmp(filename, g_dl_filename) == 0))
    {
        return false;
    }

    struct stat st = {0};
    int ret = stat(filename, &st);
    if ((ret < 0) || (st.st_size <= 0))
    {
        return false;
    }

    rt_kprintf("%s: file %s size=%d\n", __func__, filename, st.st_size);
    return true;
}

static int mp3_dl_img_callback(uint8_t *data, size_t size)
{
    int ret = 0;
    if ((data == NULL) || (size <= 0))
    {
        rt_kprintf("%s: data invalid!\n", __func__);
        g_jpg_file_downloading = false;
        if (g_user_callback)
        {
            g_user_callback(g_dl_filename, -1);
        }
        return -1;
    }
#ifdef RT_USING_DFS
    char *filename = g_dl_filename;

    /* check file exist */
    struct stat st = {0};
    ret = stat(filename, &st);
    if (ret == 0)
    {
        if ((st.st_size > 0) && (st.st_size == size))
        {
            rt_kprintf("%s: file %s already exist, skip write\n", __func__, filename);
            g_jpg_file_downloading = false;
            if (g_user_callback)
            {
                g_user_callback(g_dl_filename, 0);
            }
            return 0;
        }
    }

    /* write to file because jpg decoder do not support LV_IMAGE_SRC_SYMBOL */
    int fd;
    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0);
    if (fd >= 0)
    {
        write(fd, data, size);
        close(fd);
        rt_kprintf("%s: write %s %d bytes OK\n", __func__, filename, size);
        if (g_user_callback)
        {
            g_user_callback(g_dl_filename, 1);
        }
        ret = 0;
    }
    else
    {
        rt_kprintf("%s: open file %s failed!\n", __func__, filename);
        if (g_user_callback)
        {
            g_user_callback(g_dl_filename, -1);
        }
        ret = -1;
    }
#endif
    g_jpg_file_downloading = false;
    if (g_user_callback)
    {
        g_user_callback(g_dl_filename, ret);
    }
    return ret;
}

int mp3_dl_img(const char *url, const char *filename, mp3_dl_img_user_callback callback)
{
    RT_ASSERT(url);
    RT_ASSERT(filename);
    g_user_callback = callback;

    int ret = 0;
    rt_kprintf("%s: url=%s filename=%s\n", __func__, url, filename);
    strncpy(g_dl_filename, filename, sizeof(g_dl_filename) - 1);
    g_jpg_file_downloading = true;
    ret = mp3_network_get(url, mp3_dl_img_callback);
    return ret;
}

static lv_timer_t* pic_refresh_timer = NULL;
static void lv_pic_refresh_cb(lv_timer_t * timer)
{
    lv_obj_t *img = (lv_obj_t *)timer->user_data;
    lv_img_set_src(img, JPG_FILE);
    lv_timer_del(timer);
}

void mp3_jpg_demo(void)
{
#if 0
    /* jpg download demo */
    while (check_internet_access() == 0)
    {
        rt_kprintf("no internet, wait...\n");
        rt_thread_mdelay(2000);
    }

    lv_img_set_url(RT_NULL, JPG_URL);
#endif
    /* jpg display demo */
    lv_obj_t *img = lv_img_create(lv_scr_act());
    LV_IMG_DECLARE(img_hope_cover);
    lv_img_set_src(img, &img_hope_cover);
    pic_refresh_timer = lv_timer_create(lv_pic_refresh_cb, 10000, img);

}

#if 0
static void mp3_jpg(int argc, char **argv)
{
    lv_img_set_url(RT_NULL, JPG_URL);
}
MSH_CMD_EXPORT(mp3_jpg, MP3 jpg test)
#endif
