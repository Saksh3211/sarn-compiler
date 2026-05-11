#include "sarn_registry.h"
#include <string.h>
#define SARN_MAX_MODULES 128
static SarnModuleEntry _reg[SARN_MAX_MODULES];
static int _count = 0;
int32_t sarn_module_register(const char* name, SarnModuleInitFn init, SarnModuleFreeFn free_fn) {
    if (_count >= SARN_MAX_MODULES) return 0;
    _reg[_count].name    = name;
    _reg[_count].init    = init;
    _reg[_count].free_fn = free_fn;
    _count++;
    return 1;
}
SarnModuleEntry* sarn_module_find(const char* name) {
    for (int i = 0; i < _count; i++)
        if (strcmp(_reg[i].name, name) == 0) return &_reg[i];
    return NULL;
}
int sarn_module_count(void) { return _count; }
SarnModuleEntry* sarn_module_at(int i) { return (i >= 0 && i < _count) ? &_reg[i] : NULL; }
