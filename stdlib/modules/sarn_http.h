#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   sarn_http_get(const char* url);
char*   sarn_http_post(const char* url, const char* body, const char* ctype);
int32_t sarn_http_status(const char* url);
char*   sarn_http_get_header(const char* url, const char* header);
char*   sarn_http_post_json(const char* url, const char* json_body);
#ifdef __cplusplus
}
#endif
