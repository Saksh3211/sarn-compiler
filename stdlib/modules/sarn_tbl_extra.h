#pragma once
#include <stdint.h>
#include "../runtime/include/sarn_runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
int32_t     sarn_tbl_len_rt(SarnTable* t);
void        sarn_tbl_push(SarnTable* t, int64_t val);
void        sarn_tbl_push_f(SarnTable* t, double val);
void        sarn_tbl_push_s(SarnTable* t, const char* val);
void        sarn_tbl_pop(SarnTable* t);
int32_t     sarn_tbl_contains_s(SarnTable* t, const char* val);
int32_t     sarn_tbl_contains_i(SarnTable* t, int64_t val);
char*       sarn_tbl_keys(SarnTable* t);
void        sarn_tbl_remove_at(SarnTable* t, int32_t idx);
void        sarn_tbl_clear(SarnTable* t);
SarnTable*  sarn_tbl_merge(SarnTable* a, SarnTable* b);
SarnTable*  sarn_tbl_slice(SarnTable* t, int32_t from, int32_t to);
void        sarn_tbl_reverse(SarnTable* t);
#ifdef __cplusplus
}
#endif
