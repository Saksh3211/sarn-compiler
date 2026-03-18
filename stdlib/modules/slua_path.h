#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   slua_path_join(const char* a, const char* b);
char*   slua_path_basename(const char* path);
char*   slua_path_dirname(const char* path);
char*   slua_path_extension(const char* path);
char*   slua_path_stem(const char* path);
char*   slua_path_absolute(const char* path);
int32_t slua_path_exists(const char* path);
int32_t slua_path_is_file(const char* path);
int32_t slua_path_is_dir(const char* path);
char*   slua_path_normalize(const char* path);
#ifdef __cplusplus
}
#endif
