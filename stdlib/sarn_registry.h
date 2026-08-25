#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t (*SarnModuleInitFn)(void);
typedef void    (*SarnModuleFreeFn)(void);
typedef struct {
    const char*      name;
    SarnModuleInitFn init;
    SarnModuleFreeFn free_fn;
} 
SarnModuleEntry;
int32_t sarn_module_register(const char* name, SarnModuleInitFn init, SarnModuleFreeFn free_fn);
SarnModuleEntry* sarn_module_find(const char* name);
int sarn_module_count(void);
SarnModuleEntry* sarn_module_at(int index);
#ifdef __cplusplus
}
#endif
