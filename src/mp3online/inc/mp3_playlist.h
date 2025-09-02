
#ifndef __MP3_PLAYLIST_H__
#define __MP3_PLAYLIST_H__

#include <stdint.h>
#include "cJSON.h"

cJSON *mp3_cust_list_get(void);
int mp3_playlist_get_count(void);
char *mp3_playlist_get_song_title(int index);
char *mp3_playlist_get_song_artist(int index);
void mp3_playlist_get_song_id(int index, char *id);

int mp3_playlist_get(const char * playlist_id);

#endif
