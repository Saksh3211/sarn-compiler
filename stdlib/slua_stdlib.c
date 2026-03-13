#include "slua_stdlib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

/* ── Math ────────────────────────────────────────────────────────────────────── */

double slua_sqrt (double x) { return sqrt(x);        }
double slua_pow  (double b, double e) { return pow(b, e); }
double slua_sin  (double x) { return sin(x);          }
double slua_cos  (double x) { return cos(x);          }
double slua_tan  (double x) { return tan(x);          }
double slua_log  (double x) { return log(x);          }
double slua_log2 (double x) { return log2(x);         }
double slua_exp  (double x) { return exp(x);          }
double slua_inf  (void)     { return HUGE_VAL;         }
double slua_nan  (void)     { return nan("");}

/* ── String helpers ──────────────────────────────────────────────────────────── */

/* All returned strings are heap-allocated.
   The GC/manual memory story: callers own them. For now they leak in the
   stdlib layer — real fix is integrating with slua_alloc in a future pass. */

int32_t slua_str_len(const char* s) {
    if (!s) return 0;
    return (int32_t)strlen(s);
}

int32_t slua_str_byte(const char* s, int32_t i) {
    if (!s) return 0;
    int32_t len = (int32_t)strlen(s);
    if (i < 0 || i >= len) return 0;
    return (unsigned char)s[i];
}

char* slua_str_char(int32_t b) {
    char* buf = (char*)malloc(2);
    if (!buf) return NULL;
    buf[0] = (char)(b & 0xFF);
    buf[1] = '\0';
    return buf;
}

char* slua_str_sub(const char* s, int32_t from, int32_t to) {
    if (!s) return NULL;
    int32_t len = (int32_t)strlen(s);
    if (from < 0)    from = 0;
    if (to >= len)   to   = len - 1;
    if (from > to)   return strdup("");
    int32_t sz  = to - from + 1;
    char*   buf = (char*)malloc((size_t)sz + 1);
    if (!buf) return NULL;
    memcpy(buf, s + from, (size_t)sz);
    buf[sz] = '\0';
    return buf;
}

char* slua_int_to_str(int64_t n) {
    /* max int64 is 19 digits + sign + null */
    char* buf = (char*)malloc(24);
    if (!buf) return NULL;
    snprintf(buf, 24, "%lld", (long long)n);
    return buf;
}

char* slua_float_to_str(double x) {
    char* buf = (char*)malloc(32);
    if (!buf) return NULL;
    snprintf(buf, 32, "%g", x);
    return buf;
}

int64_t slua_str_to_int(const char* s) {
    if (!s) return 0;
    return (int64_t)strtoll(s, NULL, 10);
}

double slua_str_to_float(const char* s) {
    if (!s) return 0.0;
    return strtod(s, NULL);
}

char* slua_str_upper(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char*  buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)toupper((unsigned char)s[i]);
    buf[len] = '\0';
    return buf;
}

char* slua_str_lower(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char*  buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)s[i]);
    buf[len] = '\0';
    return buf;
}

int32_t slua_str_find(const char* haystack, const char* needle, int32_t from) {
    if (!haystack || !needle) return -1;
    int32_t hlen = (int32_t)strlen(haystack);
    if (from < 0) from = 0;
    if (from >= hlen) return -1;
    const char* found = strstr(haystack + from, needle);
    if (!found) return -1;
    return (int32_t)(found - haystack);
}

char* slua_str_trim(const char* s) {
    if (!s) return NULL;
    /* skip leading whitespace */
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    /* strip trailing whitespace */
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ── I/O ─────────────────────────────────────────────────────────────────────── */

void slua_print_str_no_newline(const char* s) {
    if (s) fputs(s, stdout);
}

void slua_write_bytes(const uint8_t* buf, int32_t len) {
    if (buf && len > 0)
        fwrite(buf, 1, (size_t)len, stdout);
}

void slua_flush(void) {
    fflush(stdout);
}

char* slua_read_line(void) {
    char   tmp[4096];
    if (!fgets(tmp, sizeof(tmp), stdin)) return strdup("");
    /* strip trailing newline */
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len-1] == '\n') tmp[--len] = '\0';
    if (len > 0 && tmp[len-1] == '\r') tmp[--len] = '\0';
    return strdup(tmp);
}

int32_t slua_read_char(void) {
    int c = fgetc(stdin);
    return (c == EOF) ? -1 : c;
}

char* slua_str_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char* buf = (char*)malloc(la + lb + 1);
    if (!buf) return NULL;
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb);
    buf[la + lb] = '\0';
    return buf;
}

void slua_io_clear(void) {
#if defined(_WIN32)
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}

void slua_io_set_color(const char* color) {
    if (!color) return;
    if      (strcmp(color, "red")     == 0) printf("\033[31m");
    else if (strcmp(color, "green")   == 0) printf("\033[32m");
    else if (strcmp(color, "yellow")  == 0) printf("\033[33m");
    else if (strcmp(color, "blue")    == 0) printf("\033[34m");
    else if (strcmp(color, "magenta") == 0) printf("\033[35m");
    else if (strcmp(color, "cyan")    == 0) printf("\033[36m");
    else if (strcmp(color, "white")   == 0) printf("\033[37m");
}

void slua_io_reset_color(void) { printf("\033[0m"); }

void slua_io_print_color(const char* msg, const char* color) {
    slua_io_set_color(color);
    if (msg) printf("%s\n", msg);
    slua_io_reset_color();
}


static SluaValue box_str(const char* s) {
    SluaValue v;
    memset(&v, 0, sizeof(v));
    v.tag = SLUA_TAG_STRING;
    v.val.ptr = s ? strdup(s) : NULL;
    return v;
}

static SluaValue str_key(const char* s) { return box_str(s); }

SluaTable* slua_tbl_new(void) { return slua_table_new(); }

void slua_tbl_iset_i64 (SluaTable* t, int64_t key, int64_t val) {
    slua_table_set(t, slua_int(key), slua_int(val));
}
void slua_tbl_iset_f64 (SluaTable* t, int64_t key, double val) {
    slua_table_set(t, slua_int(key), slua_float(val));
}
void slua_tbl_iset_str (SluaTable* t, int64_t key, const char* val) {
    slua_table_set(t, slua_int(key), box_str(val));
}
void slua_tbl_iset_bool(SluaTable* t, int64_t key, int32_t val) {
    slua_table_set(t, slua_int(key), slua_bool(val));
}

void slua_tbl_sset_i64 (SluaTable* t, const char* key, int64_t val) {
    slua_table_set(t, str_key(key), slua_int(val));
}
void slua_tbl_sset_f64 (SluaTable* t, const char* key, double val) {
    slua_table_set(t, str_key(key), slua_float(val));
}
void slua_tbl_sset_str (SluaTable* t, const char* key, const char* val) {
    slua_table_set(t, str_key(key), box_str(val));
}
void slua_tbl_sset_bool(SluaTable* t, const char* key, int32_t val) {
    slua_table_set(t, str_key(key), slua_bool(val));
}

int64_t slua_tbl_iget_i64(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    if (v.tag == SLUA_TAG_FLOAT) return (int64_t)v.val.fval;
    return v.val.ival;
}
double slua_tbl_iget_f64(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    if (v.tag == SLUA_TAG_INT) return (double)v.val.ival;
    return v.val.fval;
}
const char* slua_tbl_iget_str(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    if (v.tag != SLUA_TAG_STRING || !v.val.ptr) return "";
    return (const char*)v.val.ptr;
}
int32_t slua_tbl_iget_bool(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    return (int32_t)v.val.bits;
}

int64_t slua_tbl_sget_i64(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    if (v.tag == SLUA_TAG_FLOAT) return (int64_t)v.val.fval;
    return v.val.ival;
}
double slua_tbl_sget_f64(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    if (v.tag == SLUA_TAG_INT) return (double)v.val.ival;
    return v.val.fval;
}
const char* slua_tbl_sget_str(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    if (v.tag != SLUA_TAG_STRING || !v.val.ptr) return "";
    return (const char*)v.val.ptr;
}
int32_t slua_tbl_sget_bool(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    return (int32_t)v.val.bits;
}

