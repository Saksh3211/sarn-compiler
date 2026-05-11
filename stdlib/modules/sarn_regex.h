#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int32_t sarn_regex_match(const char* str, const char* pattern);
int32_t sarn_regex_find(const char* str, const char* pattern, int32_t from);
char*   sarn_regex_replace(const char* str, const char* pattern, const char* repl);
char*   sarn_regex_groups(const char* str, const char* pattern);
int32_t sarn_regex_count(const char* str, const char* pattern);
char*   sarn_regex_find_all(const char* str, const char* pattern);
#ifdef __cplusplus
}
#endif
