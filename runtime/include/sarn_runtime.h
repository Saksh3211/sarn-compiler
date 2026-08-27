#ifndef SARN_RUNTIME_H
#define SARN_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SARN_TAG_NULL     0x00
#define SARN_TAG_BOOL     0x01
#define SARN_TAG_INT      0x02
#define SARN_TAG_FLOAT    0x03
#define SARN_TAG_STRING   0x04
#define SARN_TAG_TABLE    0x05
#define SARN_TAG_FUNCTION 0x06
#define SARN_TAG_PTR      0x07
#define SARN_TAG_ANY      0xFF

typedef struct {
    uint8_t  tag;
    uint8_t  _pad[7];
    union {
        int64_t  ival;
        double   fval;
        void*    ptr;
        uint64_t bits;
    } val;
} SarnValue;

typedef struct {
    char*   data;
    int32_t len;
    int32_t hash;
} SarnString;

typedef struct SarnHashNode {
    SarnValue           key;
    SarnValue           val;
    struct SarnHashNode* next;
} SarnHashNode;

typedef struct {
    SarnValue*    array_part;
    int32_t       array_size;
    int32_t       array_cap;
    SarnHashNode* hash_part;
    int32_t       hash_count;
    int32_t       hash_cap;
    void*         metatable;
} SarnTable;

typedef struct {
    void* (*alloc_fn)(void* ctx, size_t size);
    void  (*free_fn) (void* ctx, void* ptr);
    void* ctx;
} SarnAllocator;

extern SarnAllocator sarn_sys_allocator;

void* sarn_alloc(size_t bytes);
void  sarn_free(void* ptr);
void* sarn_ptr_clone(const void* ptr, size_t bytes);
void* sarn_alloc_zeroed(size_t bytes);
void* sarn_realloc(void* ptr, size_t new_size);

SarnTable* sarn_table_new(void);
void       sarn_table_free(SarnTable* t);
SarnValue  sarn_table_get(SarnTable* t, SarnValue key);
void       sarn_table_set(SarnTable* t, SarnValue key, SarnValue val);
int32_t    sarn_table_length(SarnTable* t);
void       sarn_table_insert(SarnTable* t, SarnValue val);
SarnValue  sarn_table_remove(SarnTable* t, int32_t idx);

// Table setter helpers (called by compiler for table literals)
void sarn_tbl_iset_i64(SarnTable* t, int64_t key, int64_t val);
void sarn_tbl_iset_f64(SarnTable* t, int64_t key, double val);
void sarn_tbl_iset_str(SarnTable* t, int64_t key, const char* val);
void sarn_tbl_iset_bool(SarnTable* t, int64_t key, int32_t val);
void sarn_tbl_sset_i64(SarnTable* t, const char* key, int64_t val);
void sarn_tbl_sset_f64(SarnTable* t, const char* key, double val);
void sarn_tbl_sset_str(SarnTable* t, const char* key, const char* val);
void sarn_tbl_sset_bool(SarnTable* t, const char* key, int32_t val);

static inline SarnValue sarn_null(void)       { SarnValue v = {0}; return v; }
static inline SarnValue sarn_bool(int b)      { SarnValue v; v.tag = SARN_TAG_BOOL;  v.val.bits = (uint64_t)!!b; return v; }
static inline SarnValue sarn_int(int64_t i)   { SarnValue v; v.tag = SARN_TAG_INT;   v.val.ival = i;  return v; }
static inline SarnValue sarn_float(double f)  { SarnValue v; v.tag = SARN_TAG_FLOAT; v.val.fval = f;  return v; }
static inline SarnValue sarn_ptr(void* p)     { SarnValue v; v.tag = SARN_TAG_PTR;   v.val.ptr  = p;  return v; }
SarnValue sarn_string_new(const char* data, int32_t len);

SarnValue sarn_add(SarnValue a, SarnValue b);
SarnValue sarn_sub(SarnValue a, SarnValue b);
SarnValue sarn_mul(SarnValue a, SarnValue b);
SarnValue sarn_div(SarnValue a, SarnValue b);
SarnValue sarn_mod(SarnValue a, SarnValue b);
SarnValue sarn_concat(SarnValue a, SarnValue b);
int       sarn_truthy(SarnValue v);
int       sarn_equal(SarnValue a, SarnValue b);

int  sarn_is_null(SarnValue v);
void sarn_warn_null_op(const char* file, int line);
void sarn_print_value(SarnValue v);

const char* sarn_typename(SarnValue v);

_Noreturn void sarn_panic(const char* msg, const char* file, int line);
#define SARN_PANIC(msg) sarn_panic((msg), __FILE__, __LINE__)

SarnString* sarn_str_from_cstr(const char* cstr);
const char* sarn_str_cstr(SarnString* s);

double sarn_math_floor(double x);
double sarn_math_ceil(double x);
double sarn_math_sqrt(double x);
double sarn_math_abs(double x);
double sarn_math_pow(double base, double exp);
double sarn_math_min(double a, double b);
double sarn_math_max(double a, double b);

const char* sarn_num_to_str(double x);
const char* sarn_i64_to_str(int64_t x);

void sarn_print_str(const char* s);
void sarn_print_int(int64_t i);
void sarn_print_float(double f);
void sarn_print_bool(int b);
void sarn_print_null(void);
void sarn_eprint(const char* msg);

_Noreturn void sarn_exit(int code);
int64_t sarn_time_ns(void);

#ifdef __cplusplus
}
#endif

#endif