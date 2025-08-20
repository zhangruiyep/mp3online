
#ifndef __MP3_PLAYLIST_H__
#define __MP3_PLAYLIST_H__

#include "cJSON.h"

cJSON *mp3_cust_list_get(void);
int mp3_playlist_get_count(void);
char *mp3_playlist_get_song_title(int index);
char *mp3_playlist_get_song_artist(int index);

int mp3_playlist_get(int playlist_id);

#endif
