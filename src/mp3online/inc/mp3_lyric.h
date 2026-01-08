
#ifndef __MP3_LYRIC_H__
#define __MP3_LYRIC_H__

#include <stdint.h>
#include "cJSON.h"

int mp3_lyric_get(const char * track_id);
int mp3_lyric_get_by_time(int index, uint32_t secs, char *lyric_line);

#endif
