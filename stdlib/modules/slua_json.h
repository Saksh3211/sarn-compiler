#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   slua_json_encode_str(const char* s);
char*   slua_json_encode_int(int64_t n);
char*   slua_json_encode_float(double f);
char*   slua_json_encode_bool(int32_t b);
char*   slua_json_encode_null(void);
char*   slua_json_get_str(const char* json, const char* key);
int64_t slua_json_get_int(const char* json, const char* key);
double  slua_json_get_float(const char* json, const char* key);
int32_t slua_json_get_bool(const char* json, const char* key);
int32_t slua_json_has_key(const char* json, const char* key);
char*   slua_json_minify(const char* json);
char*   slua_json_get_array_item(const char* json, const char* key, int32_t index);
#ifdef __cplusplus
}
#endif
