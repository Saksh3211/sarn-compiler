#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   slua_http_get(const char* url);
char*   slua_http_post(const char* url, const char* body, const char* ctype);
int32_t slua_http_status(const char* url);
char*   slua_http_get_header(const char* url, const char* header);
char*   slua_http_post_json(const char* url, const char* json_body);
#ifdef __cplusplus
}
#endif
