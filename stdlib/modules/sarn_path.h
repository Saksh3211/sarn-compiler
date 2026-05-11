#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   sarn_path_join(const char* a, const char* b);
char*   sarn_path_basename(const char* path);
char*   sarn_path_dirname(const char* path);
char*   sarn_path_extension(const char* path);
char*   sarn_path_stem(const char* path);
char*   sarn_path_absolute(const char* path);
int32_t sarn_path_exists(const char* path);
int32_t sarn_path_is_file(const char* path);
int32_t sarn_path_is_dir(const char* path);
char*   sarn_path_normalize(const char* path);
#ifdef __cplusplus
}
#endif
