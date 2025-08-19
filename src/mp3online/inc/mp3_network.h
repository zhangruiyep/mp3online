
#ifndef __MP3_NETWORK_H__
#define __MP3_NETWORK_H__

#include <stdint.h>

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

#define POST_URL_LEN_MAX               256
#define POST_RESP_BUFSZ                2048
#define POST_HEADER_BUFSZ              2048

#define MP3_HOST_NAME           "music.163.com"

typedef int (*mp3_nw_rsp_data_callback)(uint8_t *data, size_t len);

typedef enum
{
    MP3_NW_CMD_GET,
    MP3_NW_CMD_POST,
} mp3_nw_cmd_t;

typedef struct
{
    mp3_nw_cmd_t cmd;
    const char *url;
    const uint8_t *post_data;
    size_t post_data_len;
    mp3_nw_rsp_data_callback callback;
} mp3_nw_msg_t;

int mp3_network_post(const char *url, const uint8_t *post_data, size_t post_data_len, mp3_nw_rsp_data_callback callback);

#endif