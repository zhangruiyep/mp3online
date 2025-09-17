
#include <rtthread.h>
#include <string.h>
#include <webclient.h>
#include <lvgl.h>
#ifdef RT_USING_DFS
#include <dfs_posix.h>
#endif
#include "mp3_mem.h"
#include "mp3_jpg.h"

#define GET_HEADER_BUFSZ        2048        //头部大小
#define GET_RESP_BUFSZ          8192        //响应缓冲区大小
#define GET_URL_LEN_MAX         256         //网址最大长度

#define JPG_URL "http://sample-files.com/downloads/images/jpg/thumbnail_150x150_10.5kb.jpg"
#define JPG_FILE "/mp3_temp.jpg"

//static char *buffer = RT_NULL;
extern int check_internet_access(void);

int mp3_dl_img(const char *url, const char *filename)
{
    int ret = -1;
    int resp_status;
    struct webclient_session *session = RT_NULL;
    int content_length = -1, bytes_read = 0;
    int content_pos = 0;
    char *buffer = RT_NULL;

    rt_kprintf("%s url=%s filename=%s\n", __func__, url, filename);

    /* 创建会话并且设置响应的大小 */
    session = webclient_session_create(GET_HEADER_BUFSZ);
    if (session == RT_NULL)
    {
        rt_kprintf("No memory for get header!\n");
        goto __exit;
    }

    /* 发送 GET 请求使用默认的头部 */
    if ((resp_status = webclient_get(session, url)) != 200)
    {
        rt_kprintf("webclient GET request failed, response(%d) error.\n", resp_status);
        goto __exit;
    }

    content_length = webclient_content_length_get(session);
    if (content_length > 0)
    {
        rt_kprintf("content_length==%d\n", content_length);
        buffer = mp3_mem_malloc(content_length);
        RT_ASSERT(buffer);
        bytes_read = webclient_read(session, buffer, content_length);
        rt_kprintf("bytes_read=%d\n", bytes_read);
        if (bytes_read < content_length)
        {
            rt_kprintf("%s bytes_read=%d err!!\n", __func__, bytes_read);
            goto __exit;
        }
#ifdef RT_USING_DFS
        /* write to file because jpg decoder do not support LV_IMAGE_SRC_SYMBOL */
        int fd;
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0);
        if (fd >= 0)
        {
            write(fd, buffer, bytes_read);
            close(fd);
            rt_kprintf("%s: write %s %d bytes OK\n", __func__, filename, bytes_read);
            ret = 0;
        }
        else
        {
            rt_kprintf("open file:%s failed!\n", filename);
            goto __exit;
        }
#if 0
        if (RT_NULL == img)
        {
            rt_kprintf("%s: create img\n", __func__);
            img = lv_img_create(lv_scr_act());
            RT_ASSERT(img);
        }
        else
        {
            rt_kprintf("%s: img=%x\n", __func__, img);
        }
        rt_kprintf("%s %d: img=%x thread=%s", __func__, __LINE__, img, rt_thread_self()->name);
        lv_img_set_src(img, JPG_FILE);
        //rt_kprintf("%s pic size=%d x %d\n", __func__, lv_obj_get_width(wp), lv_obj_get_height(wp));
#endif
#endif
    }
    else
    {
        rt_kprintf("content_length==0! return NULL\n");
    }

__exit:

    /* 关闭会话 */
    if (session != RT_NULL)
        webclient_close(session);

    if (buffer != RT_NULL)
        mp3_mem_free(buffer);

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
