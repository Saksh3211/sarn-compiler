#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*SarnThreadFn)(void* arg);
int64_t sarn_thread_create(SarnThreadFn fn, void* arg);
int32_t sarn_thread_join(int64_t id);
int32_t sarn_thread_detach(int64_t id);
int32_t sarn_thread_alive(int64_t id);
void    sarn_thread_sleep_ms(int64_t ms);
int64_t sarn_thread_self_id(void);
#ifdef __cplusplus
}
#endif
