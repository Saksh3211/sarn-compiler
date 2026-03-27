#pragma once
#include <stdint.h>
#include "../runtime/include/slua_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
int32_t     slua_tbl_len_rt(SluaTable* t);
void        slua_tbl_push(SluaTable* t, int64_t val);
void        slua_tbl_push_f(SluaTable* t, double val);
void        slua_tbl_push_s(SluaTable* t, const char* val);
void        slua_tbl_pop(SluaTable* t);
int32_t     slua_tbl_contains_s(SluaTable* t, const char* val);
int32_t     slua_tbl_contains_i(SluaTable* t, int64_t val);
char*       slua_tbl_keys(SluaTable* t);
void        slua_tbl_remove_at(SluaTable* t, int32_t idx);
void        slua_tbl_clear(SluaTable* t);
SluaTable*  slua_tbl_merge(SluaTable* a, SluaTable* b);
SluaTable*  slua_tbl_slice(SluaTable* t, int32_t from, int32_t to);
void        slua_tbl_reverse(SluaTable* t);
#ifdef __cplusplus
}
#endif
