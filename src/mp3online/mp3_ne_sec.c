
/* provider ne */

#include <rtthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <cJSON.h>
#include "mbedtls/base64.h"
#include "mbedtls/cipher.h"
#include "mbedtls/bignum.h"

#define MP3_NE_DEBUG
//#define RSA_DEBUG
//#define WEAPI_DEBUG

#ifdef MP3_NE_DEBUG
#define mp3_log(...) rt_kprintf(__VA_ARGS__)
#else
#define mp3_log(...)
#endif
#define mp3_err(...) rt_kprintf(__VA_ARGS__)

#define ALIGN_16(x) (((x) + 0xf) & ~0xf)
#define mp3_free(ptr)   do{\
    if(ptr)\
        free(ptr);\
}while(0)

static mbedtls_cipher_context_t g_cipher_ctx;

static int mp3_text_dump(char *text, int len)
{
    RT_ASSERT(text);
    if (len >= 8)
    {
        mp3_log("head:");
        for (int i = 0; i < 8; i++) {
            mp3_log("%c", text[i]);
        }
        mp3_log("\ntail:");
        for (int i = len - 8; i < len; i++) {
            mp3_log("%c", text[i]);
        }
        mp3_log("\n");
    }
    else if (len > 0)
    {
        for (int i = 0; i < len; i++) {
            mp3_log("%c", text[i]);
        }
        mp3_log("\n");
    }
}

static int mp3_bin2hex(const uint8_t *bin, int len, char *hex)
{
    for (int i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bin[i]);
    }
    return 0;
}

uint8_t * ne_create_secret_key(int size) {
    if (size % 4 != 0)
    {
        mp3_err("size only support 4 alignment for now\n");
        return NULL;
    }

    uint8_t* result = (uint8_t*)malloc(size+1);
    RT_ASSERT(result);
    memset(result, 0, size+1);

    srand((unsigned int)time(NULL));

    for (int i = 0; i < size/8; i++)
    {
        int r = rand();
        sprintf(result + i * 8, "%08x", r);
    }

    return result;
}

static char* ne_base64_encode(const unsigned char* input, int input_len, int* output_len)
{
    int output_buff_len = ALIGN_16(input_len*4/3+2);  //about 4/3 of input
    char *output = (char*)malloc(output_buff_len);
    RT_ASSERT(output);

    int ret = mbedtls_base64_encode((unsigned char*)output, output_buff_len, output_len, input, input_len);
    if (ret != 0)
    {
        mp3_err("%s b64 fail %d\n", __func__, ret);
        free(output);
        return NULL;
    }

    return output;
}

static uint8_t* ne_aes_cbc_encrypt(const char* text, const unsigned char* sec_key, int* output_len) {
    int ret = 0;
    const mbedtls_cipher_info_t *cipher_info = NULL;

    mbedtls_cipher_init(&g_cipher_ctx);
    cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_CBC);
    RT_ASSERT(cipher_info);
    ret = mbedtls_cipher_setup(&g_cipher_ctx, cipher_info);
    RT_ASSERT(ret==0);
    ret = mbedtls_cipher_setkey(&g_cipher_ctx, sec_key, 128, MBEDTLS_ENCRYPT);
    RT_ASSERT(ret==0);
    ret = mbedtls_cipher_set_iv(&g_cipher_ctx, "0102030405060708", 16);
    RT_ASSERT(ret==0);
    ret = mbedtls_cipher_reset(&g_cipher_ctx);
    RT_ASSERT(ret==0);

    int output_buf_len = ALIGN_16(strlen(text)+1)+1;
    uint8_t* output = (uint8_t*)malloc(output_buf_len);
    RT_ASSERT(output);
    //mp3_log("%s:output=%x len=%d\n", __func__, output, output_buf_len);
    memset(output, 0, output_buf_len);

    int out_len = 0;
    *output_len = 0;
    ret = mbedtls_cipher_update(&g_cipher_ctx, text, strlen(text), output, &out_len);
    RT_ASSERT(ret==0);
    *output_len = out_len;
    ret = mbedtls_cipher_finish(&g_cipher_ctx, output + out_len, &out_len);
    RT_ASSERT(ret==0);
    *output_len += out_len;

    mbedtls_cipher_free(&g_cipher_ctx);

    return output;
}

const char* modulus =
    "00e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b72"\
    "5152b3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312ecbd"\
    "a92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d813cfe48"\
    "75d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7";
const char* pubKey = "010001";

static char* ne_rsa_encrypt(const char *input)
{
    int ret = 0;
    char *reverse = (char*)malloc(strlen(input) + 1);
    RT_ASSERT(reverse);
    memset(reverse, 0, strlen(input) + 1);
    for (int i = strlen(input) - 1; i >= 0; i--)
    {
        reverse[strlen(input) - 1 - i] = input[i];
    }
    reverse[strlen(input)] = '\0';
#ifdef RSA_DEBUG
    mp3_text_dump(reverse, strlen(reverse));
#endif

    mbedtls_mpi n, e, b, enc, rr;
    mbedtls_mpi_init(&n);
    mbedtls_mpi_init(&e);
    mbedtls_mpi_init(&b);
    mbedtls_mpi_init(&enc);
    mbedtls_mpi_init(&rr);

#define RSA_OUT_BUF_LEN 260
    size_t olen = 0;
    char *output_str = (char*)malloc(RSA_OUT_BUF_LEN);
    RT_ASSERT(output_str);
    memset(output_str, 0, RSA_OUT_BUF_LEN);

    ret = mbedtls_mpi_read_string(&n, 16, modulus);
    RT_ASSERT(ret == 0);
    ret = mbedtls_mpi_read_string(&e, 16, pubKey);
    RT_ASSERT(ret == 0);

#ifdef RSA_DEBUG
    ret = mbedtls_mpi_write_string(&n, 16, output_str, RSA_OUT_BUF_LEN, &olen);
    RT_ASSERT(ret == 0);
    mp3_log("%s: n olen=%d\n", __func__, olen);
    mp3_text_dump(output_str, strlen(output_str));

    ret = mbedtls_mpi_write_string(&e, 16, output_str, RSA_OUT_BUF_LEN, &olen);
    RT_ASSERT(ret == 0);
    mp3_log("%s: e olen=%d\n", __func__, olen);
    mp3_text_dump(output_str, strlen(output_str));
#endif

    char *b_str = (char*)malloc(40);
    RT_ASSERT(b_str);
    memset(b_str, 0, 40);
    mp3_bin2hex(reverse, strlen(input), b_str);
    ret = mbedtls_mpi_read_string(&b, 16, b_str);
    RT_ASSERT(ret == 0);
    mp3_free(reverse);
    mp3_free(b_str);

#ifdef RSA_DEBUG
    ret = mbedtls_mpi_write_string(&b, 16, output_str, RSA_OUT_BUF_LEN, &olen);
    RT_ASSERT(ret == 0);
    mp3_log("%s: b olen=%d\n", __func__, olen);
    mp3_text_dump(output_str, strlen(output_str));
#endif

    ret = mbedtls_mpi_exp_mod(&enc, &b, &e, &n, &rr);
    RT_ASSERT(ret == 0);

    ret = mbedtls_mpi_write_string(&enc, 16, output_str, RSA_OUT_BUF_LEN, &olen);
    RT_ASSERT(ret == 0);

#ifdef RSA_DEBUG
    mp3_log("%s: olen=%d\n", __func__, olen);
    mp3_text_dump(output_str, strlen(output_str));
#endif

    return output_str;
}

static cJSON* weapi(cJSON* json) {
    const uint8_t* nonce = "0CoJUm6Qyw8W8jud";
    char* text = cJSON_PrintUnformatted(json); // eslint-disable-line no-param-reassign
    RT_ASSERT(text);

#ifdef WEAPI_DEBUG
    mp3_log("%s:text=%s\n", __func__, text);
#endif

    char* sec_key = ne_create_secret_key(16);
    //char* sec_key = "2bc9a7a9f7eadda4"; //test key
#ifdef WEAPI_DEBUG
    mp3_log("%s:sec_key=%s\n", __func__, sec_key);
#endif

    int out_len = 0;
    uint8_t *aes_enc_data = ne_aes_cbc_encrypt(text, nonce, &out_len);
#ifdef WEAPI_DEBUG
    mp3_log("%s:aes_enc_data len=%d\n", __func__, out_len);
#endif
    if (text)
    {
        cJSON_free(text);
    }

    int b64_len = 0;
    char *b64_enc_text = ne_base64_encode(aes_enc_data, out_len, &b64_len);
    RT_ASSERT(b64_enc_text);
    mp3_free(aes_enc_data);
#ifdef WEAPI_DEBUG
    //mp3_log("%s:b64 len=%d\n", __func__, b64_len);
    mp3_text_dump(b64_enc_text, b64_len);
#endif

    uint8_t *aes_enc_data_2 = ne_aes_cbc_encrypt(b64_enc_text, sec_key, &out_len);
#ifdef WEAPI_DEBUG
    //mp3_log("%s:aes_enc_data_2=%x\n", __func__, aes_enc_data_2);
    //mp3_log("%s:aes_enc_data_2 len=%d\n", __func__, out_len);
#endif
    mp3_free(b64_enc_text);

    int b64_len_2 = 0;
    char *b64_enc_text_2 = ne_base64_encode(aes_enc_data_2, out_len, &b64_len);
    RT_ASSERT(b64_enc_text_2);
#ifdef WEAPI_DEBUG
    //mp3_log("%s:b64 len=%d\n", __func__, b64_len);
    mp3_text_dump(b64_enc_text_2, b64_len);
    //mp3_log("%s:aes_enc_data_2=%x\n", __func__, aes_enc_data_2);
#endif
    mp3_free(aes_enc_data_2);

    char* enc_sec_key = ne_rsa_encrypt(sec_key);
    RT_ASSERT(enc_sec_key);
#ifdef WEAPI_DEBUG
    mp3_text_dump(enc_sec_key, strlen(enc_sec_key));
#endif

    cJSON *data = cJSON_CreateObject();
    RT_ASSERT(data);
    cJSON_AddStringToObject(data, "params", b64_enc_text_2);
    cJSON_AddStringToObject(data, "encSecKey", enc_sec_key);
    mp3_free(b64_enc_text_2);
    mp3_free(enc_sec_key);

#ifdef WEAPI_DEBUG
    mp3_log("%s: done\n", __func__);
#endif
    return data;
}





static void mp3_ne_test(int argc, char **argv)
{
    if (strcmp(argv[1], "weapi") == 0)
    {
        cJSON *data = cJSON_Parse("{\"id\":\"2819914042\",\"offset\":0,\"total\":true,\"limit\":1000,\"n\":1000,\"csrf_token\":\"\"}");
        cJSON *out = weapi(data);
        cJSON_Delete(data);
        cJSON_Delete(out);
    }
}
MSH_CMD_EXPORT(mp3_ne_test, MP3 NE api test);
