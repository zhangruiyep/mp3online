
#ifndef MP3_NE_SEC_H
#define MP3_NE_SEC_H

#include <stdint.h>

/* random secret key, 4 bytes aligned */
uint8_t * ne_create_secret_key(int size);

#endif
