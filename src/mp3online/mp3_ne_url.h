
#ifndef MP3_NE_URL_H
#define MP3_NE_URL_H

#include "cJSON.h"

void ne_init_cookie(void);
char *ne_get_cookie(void);
void ne_set_cookie(char *cookie);

char *cJSON_to_query_string(cJSON *json);

#endif
