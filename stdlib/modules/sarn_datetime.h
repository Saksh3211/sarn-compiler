#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t sarn_datetime_now(void);
char*   sarn_datetime_format(int64_t ts, const char* fmt);
int64_t sarn_datetime_parse(const char* str, const char* fmt);
int64_t sarn_datetime_diff(int64_t a, int64_t b);
int64_t sarn_datetime_add(int64_t ts, int64_t seconds);
char*   sarn_datetime_now_str(const char* fmt);
int32_t sarn_datetime_year(int64_t ts);
int32_t sarn_datetime_month(int64_t ts);
int32_t sarn_datetime_day(int64_t ts);
int32_t sarn_datetime_hour(int64_t ts);
int32_t sarn_datetime_minute(int64_t ts);
int32_t sarn_datetime_second(int64_t ts);
#ifdef __cplusplus
}
#endif
