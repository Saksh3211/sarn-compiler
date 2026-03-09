#ifndef SLUA_STDLIB_H
#define SLUA_STDLIB_H

#include <stdint.h>
#include <stddef.h>

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

void     slua_print_str_no_newline (const char* s);
void     slua_write_bytes          (const uint8_t* buf, int32_t len);
void     slua_flush                (void);
char*    slua_read_line            (void);
int32_t  slua_read_char            (void);

void     slua_io_clear             (void);
void     slua_io_print_color       (const char* text, const char* color);
void     slua_io_set_color         (const char* color);
void     slua_io_reset_color       (void);

#ifdef __cplusplus
}
#endif

#endif /* SLUA_STDLIB_H */
