#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t slua_sync_mutex_new(void);
int32_t slua_sync_mutex_lock(int64_t id);
int32_t slua_sync_mutex_unlock(int64_t id);
int32_t slua_sync_mutex_trylock(int64_t id);
int32_t slua_sync_mutex_free(int64_t id);
#ifdef __cplusplus
}
#endif
