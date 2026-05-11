#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   sarn_json_encode_str(const char* s);
char*   sarn_json_encode_int(int64_t n);
char*   sarn_json_encode_float(double f);
char*   sarn_json_encode_bool(int32_t b);
char*   sarn_json_encode_null(void);
char*   sarn_json_get_str(const char* json, const char* key);
int64_t sarn_json_get_int(const char* json, const char* key);
double  sarn_json_get_float(const char* json, const char* key);
int32_t sarn_json_get_bool(const char* json, const char* key);
int32_t sarn_json_has_key(const char* json, const char* key);
char*   sarn_json_minify(const char* json);
char*   sarn_json_get_array_item(const char* json, const char* key, int32_t index);
double  sarn_json_get_nested_float(const char* json, const char* outer, const char* inner);
int64_t sarn_json_get_nested_int(const char* json, const char* outer, const char* inner);
char*   sarn_json_get_nested_str(const char* json, const char* outer, const char* inner);
#ifdef __cplusplus
}
#endif
