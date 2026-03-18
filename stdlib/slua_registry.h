#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t (*SluaModuleInitFn)(void);
typedef void    (*SluaModuleFreeFn)(void);
typedef struct {
    const char*      name;
    SluaModuleInitFn init;
    SluaModuleFreeFn free_fn;
} SluaModuleEntry;
int32_t            slua_module_register(const char* name, SluaModuleInitFn init, SluaModuleFreeFn free_fn);
SluaModuleEntry*   slua_module_find(const char* name);
int                slua_module_count(void);
SluaModuleEntry*   slua_module_at(int index);
#ifdef __cplusplus
}
#endif
