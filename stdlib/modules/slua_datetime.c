#include "slua_datetime.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int64_t slua_datetime_now(void) { return (int64_t)time(NULL); }
char* slua_datetime_format(int64_t ts, const char* fmt) {
    time_t t = (time_t)ts; struct tm* ti = localtime(&t);
    char* b = (char*)malloc(512); if (!b) return strdup("");
    strftime(b, 512, fmt, ti); return b;
}
int64_t slua_datetime_parse(const char* str, const char* fmt) {
    struct tm ti; memset(&ti, 0, sizeof(ti));
    int y=1970,mo=1,d=1,h=0,mi=0,s=0;
    sscanf(str, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s);
    ti.tm_year=y-1900; ti.tm_mon=mo-1; ti.tm_mday=d;
    ti.tm_hour=h; ti.tm_min=mi; ti.tm_sec=s;
    return (int64_t)mktime(&ti);
}
int64_t slua_datetime_diff(int64_t a, int64_t b) { return a - b; }
int64_t slua_datetime_add(int64_t ts, int64_t sec) { return ts + sec; }
char*   slua_datetime_now_str(const char* fmt) { return slua_datetime_format(slua_datetime_now(), fmt); }
static struct tm* _tm(int64_t ts) { time_t t = (time_t)ts; return localtime(&t); }
int32_t slua_datetime_year(int64_t ts)   { return _tm(ts)->tm_year + 1900; }
int32_t slua_datetime_month(int64_t ts)  { return _tm(ts)->tm_mon + 1; }
int32_t slua_datetime_day(int64_t ts)    { return _tm(ts)->tm_mday; }
int32_t slua_datetime_hour(int64_t ts)   { return _tm(ts)->tm_hour; }
int32_t slua_datetime_minute(int64_t ts) { return _tm(ts)->tm_min; }
int32_t slua_datetime_second(int64_t ts) { return _tm(ts)->tm_sec; }
