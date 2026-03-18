#include "slua_registry.h"
#include <string.h>
#define SLUA_MAX_MODULES 128
static SluaModuleEntry _reg[SLUA_MAX_MODULES];
static int _count = 0;
int32_t slua_module_register(const char* name, SluaModuleInitFn init, SluaModuleFreeFn free_fn) {
    if (_count >= SLUA_MAX_MODULES) return 0;
    _reg[_count].name    = name;
    _reg[_count].init    = init;
    _reg[_count].free_fn = free_fn;
    _count++;
    return 1;
}
SluaModuleEntry* slua_module_find(const char* name) {
    for (int i = 0; i < _count; i++)
        if (strcmp(_reg[i].name, name) == 0) return &_reg[i];
    return NULL;
}
int slua_module_count(void) { return _count; }
SluaModuleEntry* slua_module_at(int i) { return (i >= 0 && i < _count) ? &_reg[i] : NULL; }
