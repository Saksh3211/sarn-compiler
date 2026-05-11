#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   sarn_crypto_sha256(const char* data);
char*   sarn_crypto_md5(const char* data);
char*   sarn_crypto_base64_encode(const char* data, int32_t len);
char*   sarn_crypto_base64_decode(const char* b64);
char*   sarn_crypto_hex_encode(const char* data, int32_t len);
char*   sarn_crypto_hex_decode(const char* hex);
int32_t sarn_crypto_hex_decode_len(const char* hex);
uint32_t sarn_crypto_crc32(const char* data, int32_t len);
char*   sarn_crypto_hmac_sha256(const char* key, const char* data);
char*   sarn_crypto_xor(const char* data, int32_t len, const char* key, int32_t keylen);
#ifdef __cplusplus
}
#endif
