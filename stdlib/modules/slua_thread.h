#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*SluaThreadFn)(void* arg);
int64_t slua_thread_create(SluaThreadFn fn, void* arg);
int32_t slua_thread_join(int64_t id);
int32_t slua_thread_detach(int64_t id);
int32_t slua_thread_alive(int64_t id);
void    slua_thread_sleep_ms(int64_t ms);
int64_t slua_thread_self_id(void);
#ifdef __cplusplus
}
#endif
