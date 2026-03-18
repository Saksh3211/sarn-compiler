#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t slua_datetime_now(void);
char*   slua_datetime_format(int64_t ts, const char* fmt);
int64_t slua_datetime_parse(const char* str, const char* fmt);
int64_t slua_datetime_diff(int64_t a, int64_t b);
int64_t slua_datetime_add(int64_t ts, int64_t seconds);
char*   slua_datetime_now_str(const char* fmt);
int32_t slua_datetime_year(int64_t ts);
int32_t slua_datetime_month(int64_t ts);
int32_t slua_datetime_day(int64_t ts);
int32_t slua_datetime_hour(int64_t ts);
int32_t slua_datetime_minute(int64_t ts);
int32_t slua_datetime_second(int64_t ts);
#ifdef __cplusplus
}
#endif
