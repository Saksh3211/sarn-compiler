#ifndef SLUA_STDLIB_H
#define SLUA_STDLIB_H

#include <stdint.h>
#include <stddef.h>
#include "../runtime/include/slua_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

double   slua_sqrt (double x);
double   slua_pow  (double base, double exp);
double   slua_sin  (double x);
double   slua_cos  (double x);
double   slua_tan  (double x);
double   slua_log  (double x);
double   slua_log2 (double x);
double   slua_exp  (double x);
double   slua_inf  (void);
double   slua_nan  (void);

int32_t  slua_str_len      (const char* s);
int32_t  slua_str_byte     (const char* s, int32_t i);
char*    slua_str_char     (int32_t b);
char*    slua_str_sub      (const char* s, int32_t from, int32_t to);
char*    slua_int_to_str   (int64_t n);
char*    slua_float_to_str (double x);
int64_t  slua_str_to_int   (const char* s);
double   slua_str_to_float (const char* s);
char*    slua_str_upper    (const char* s);
char*    slua_str_lower    (const char* s);
int32_t  slua_str_find     (const char* haystack, const char* needle, int32_t from);
char*    slua_str_trim     (const char* s);

char*    slua_str_concat   (const char* a, const char* b);

SluaTable* slua_tbl_new(void);

void        slua_tbl_iset_i64 (SluaTable* t, int64_t key, int64_t val);
void        slua_tbl_iset_f64 (SluaTable* t, int64_t key, double  val);
void        slua_tbl_iset_str (SluaTable* t, int64_t key, const char* val);
void        slua_tbl_iset_bool(SluaTable* t, int64_t key, int32_t val);

void        slua_tbl_sset_i64 (SluaTable* t, const char* key, int64_t val);
void        slua_tbl_sset_f64 (SluaTable* t, const char* key, double  val);
void        slua_tbl_sset_str (SluaTable* t, const char* key, const char* val);
void        slua_tbl_sset_bool(SluaTable* t, const char* key, int32_t val);

int64_t     slua_tbl_iget_i64 (SluaTable* t, int64_t key);
double      slua_tbl_iget_f64 (SluaTable* t, int64_t key);
const char* slua_tbl_iget_str (SluaTable* t, int64_t key);
int32_t     slua_tbl_iget_bool(SluaTable* t, int64_t key);

int64_t     slua_tbl_sget_i64 (SluaTable* t, const char* key);
double      slua_tbl_sget_f64 (SluaTable* t, const char* key);
const char* slua_tbl_sget_str (SluaTable* t, const char* key);
int32_t     slua_tbl_sget_bool(SluaTable* t, const char* key);

void     slua_print_str_no_newline (const char* s);
void     slua_write_bytes          (const uint8_t* buf, int32_t len);
void     slua_flush                (void);
char*    slua_read_line            (void);
int32_t  slua_read_char            (void);
void     slua_io_clear             (void);
void     slua_io_set_color         (const char* color);
void     slua_io_reset_color       (void);
void     slua_io_print_color       (const char* msg, const char* color);

#ifdef __cplusplus
}
#endif

#endif