#ifndef SLUA_RUNTIME_H
#define SLUA_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLUA_TAG_NULL     0x00
#define SLUA_TAG_BOOL     0x01
#define SLUA_TAG_INT      0x02
#define SLUA_TAG_FLOAT    0x03
#define SLUA_TAG_STRING   0x04
#define SLUA_TAG_TABLE    0x05
#define SLUA_TAG_FUNCTION 0x06
#define SLUA_TAG_PTR      0x07
#define SLUA_TAG_ANY      0xFF

typedef struct {
    uint8_t  tag;
    uint8_t  _pad[7];
    union {
        int64_t  ival;
        double   fval;
        void*    ptr;
        uint64_t bits;
    } val;
} SluaValue;

typedef struct {
    char*   data;
    int32_t len;
    int32_t hash;
} SluaString;

typedef struct SluaHashNode {
    SluaValue           key;
    SluaValue           val;
    struct SluaHashNode* next;
} SluaHashNode;

typedef struct {
    SluaValue*    array_part;
    int32_t       array_size;
    int32_t       array_cap;
    SluaHashNode* hash_part;
    int32_t       hash_count;
    int32_t       hash_cap;
    void*         metatable;
} SluaTable;

typedef struct {
    void* (*alloc_fn)(void* ctx, size_t size);
    void  (*free_fn) (void* ctx, void* ptr);
    void* ctx;
} SluaAllocator;

extern SluaAllocator slua_sys_allocator;

void* slua_alloc(size_t bytes);
void  slua_free(void* ptr);
void* slua_alloc_zeroed(size_t bytes);
void* slua_realloc(void* ptr, size_t new_size);

SluaTable* slua_table_new(void);
void       slua_table_free(SluaTable* t);
SluaValue  slua_table_get(SluaTable* t, SluaValue key);
void       slua_table_set(SluaTable* t, SluaValue key, SluaValue val);
int32_t    slua_table_length(SluaTable* t);
void       slua_table_insert(SluaTable* t, SluaValue val);
SluaValue  slua_table_remove(SluaTable* t, int32_t idx);

static inline SluaValue slua_null(void)       { SluaValue v = {0}; return v; }
static inline SluaValue slua_bool(int b)      { SluaValue v; v.tag = SLUA_TAG_BOOL;  v.val.bits = (uint64_t)!!b; return v; }
static inline SluaValue slua_int(int64_t i)   { SluaValue v; v.tag = SLUA_TAG_INT;   v.val.ival = i;  return v; }
static inline SluaValue slua_float(double f)  { SluaValue v; v.tag = SLUA_TAG_FLOAT; v.val.fval = f;  return v; }
static inline SluaValue slua_ptr(void* p)     { SluaValue v; v.tag = SLUA_TAG_PTR;   v.val.ptr  = p;  return v; }
SluaValue slua_string_new(const char* data, int32_t len);

SluaValue slua_add(SluaValue a, SluaValue b);
SluaValue slua_sub(SluaValue a, SluaValue b);
SluaValue slua_mul(SluaValue a, SluaValue b);
SluaValue slua_div(SluaValue a, SluaValue b);
SluaValue slua_mod(SluaValue a, SluaValue b);
SluaValue slua_concat(SluaValue a, SluaValue b);
int       slua_truthy(SluaValue v);
int       slua_equal(SluaValue a, SluaValue b);

int  slua_is_null(SluaValue v);
void slua_warn_null_op(const char* file, int line);
void slua_print_value(SluaValue v);

const char* slua_typename(SluaValue v);

_Noreturn void slua_panic(const char* msg, const char* file, int line);
#define SLUA_PANIC(msg) slua_panic((msg), __FILE__, __LINE__)

int         slua_str_len(SluaString* s);
SluaString* slua_str_from_cstr(const char* cstr);
const char* slua_str_cstr(SluaString* s);

double slua_math_floor(double x);
double slua_math_ceil(double x);
double slua_math_sqrt(double x);
double slua_math_abs(double x);
double slua_math_pow(double base, double exp);
double slua_math_min(double a, double b);
double slua_math_max(double a, double b);
double slua_sqrt(double x);
double slua_pow(double b, double e);
double slua_sin(double x);
double slua_cos(double x);
double slua_tan(double x);
double slua_log(double x);
double slua_log2(double x);
double slua_exp(double x);
double slua_inf(void);
double slua_nan(void);

const char* slua_num_to_str(double x);
const char* slua_i64_to_str(int64_t x);

void slua_print_str(const char* s);
void slua_print_int(int64_t i);
void slua_print_float(double f);
void slua_print_bool(int b);
void slua_print_null(void);
void slua_eprint(const char* msg);

_Noreturn void slua_exit(int code);
int64_t slua_time_ns(void);

#ifdef __cplusplus
}
#endif

#endif