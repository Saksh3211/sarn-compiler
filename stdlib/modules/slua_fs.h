#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
char*   slua_fs_read_all(const char* path);
int32_t slua_fs_write(const char* path, const char* data);
int32_t slua_fs_append(const char* path, const char* data);
int32_t slua_fs_exists(const char* path);
int32_t slua_fs_delete(const char* path);
int32_t slua_fs_mkdir(const char* path);
int32_t slua_fs_rename(const char* from, const char* to);
int64_t slua_fs_size(const char* path);
char*   slua_fs_listdir(const char* path);
int64_t slua_fs_open(const char* path, const char* mode);
int32_t slua_fs_close(int64_t handle);
char*   slua_fs_readline(int64_t handle);
int32_t slua_fs_writeh(int64_t handle, const char* data);
int32_t slua_fs_flush(int64_t handle);
int32_t slua_fs_copy(const char* src, const char* dst);
#ifdef __cplusplus
}
#endif
