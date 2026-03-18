#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t slua_process_run(const char* cmd);
char*   slua_process_output(const char* cmd);
int64_t slua_process_spawn(const char* cmd);
int32_t slua_process_wait(int64_t id);
int32_t slua_process_kill(int64_t id);
int32_t slua_process_alive(int64_t id);
#ifdef __cplusplus
}
#endif
