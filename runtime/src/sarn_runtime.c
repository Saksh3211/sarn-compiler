#include "../include/sarn_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <time.h>
#endif

static void* sys_alloc_fn(void* ctx, size_t size) { (void)ctx; return malloc(size); }
static void  sys_free_fn (void* ctx, void* ptr)   { (void)ctx; free(ptr); }

SarnAllocator sarn_sys_allocator = {
    .alloc_fn = sys_alloc_fn,
    .free_fn  = sys_free_fn,
    .ctx      = NULL,
};

void* sarn_alloc       (size_t bytes)              { return malloc(bytes); }
void  sarn_free        (void* ptr)                 { free(ptr); }
void* sarn_ptr_clone  (const void* ptr, size_t bytes) {
    if (!ptr) return NULL;
    void* copy = malloc(bytes);
    if (!copy) SARN_PANIC("out of memory");
    memcpy(copy, ptr, bytes);
    return copy;
}
void* sarn_alloc_zeroed(size_t bytes)              { return calloc(1, bytes); }
void* sarn_realloc     (void* ptr, size_t new_size){ return realloc(ptr, new_size); }

void sarn_panic(const char* msg, const char* file, int line) {
    fprintf(stderr, "\n[PANIC] %s\n  at %s:%d\n", msg, file, line);
    abort();
}

void sarn_print_value(SarnValue v) {
    switch (v.tag) {
        case SARN_TAG_NULL:     printf("-null-"); break;
        case SARN_TAG_BOOL:     printf("%s", v.val.bits ? "true" : "false"); break;
        case SARN_TAG_INT:      printf("%lld", (long long)v.val.ival); break;
        case SARN_TAG_FLOAT:    printf("%g", v.val.fval); break;
        case SARN_TAG_STRING: {
            SarnString* s = (SarnString*)v.val.ptr;
            if (s && s->data) fwrite(s->data, 1, (size_t)s->len, stdout);
            break;
        }
        case SARN_TAG_TABLE:    printf("<table:%p>",    v.val.ptr); break;
        case SARN_TAG_FUNCTION: printf("<function:%p>", v.val.ptr); break;
        case SARN_TAG_PTR:      printf("<ptr:%p>",      v.val.ptr); break;
        default:                printf("<unknown>"); break;
    }
}

const char* sarn_typename(SarnValue v) {
    switch (v.tag) {
        case SARN_TAG_NULL:     return "null";
        case SARN_TAG_BOOL:     return "bool";
        case SARN_TAG_INT:      return "int";
        case SARN_TAG_FLOAT:    return "number";
        case SARN_TAG_STRING:   return "string";
        case SARN_TAG_TABLE:    return "table";
        case SARN_TAG_FUNCTION: return "function";
        case SARN_TAG_PTR:      return "ptr";
        default:                return "unknown";
    }
}

int sarn_is_null(SarnValue v) { return v.tag == SARN_TAG_NULL; }
int sarn_truthy (SarnValue v) {
    return !(v.tag == SARN_TAG_NULL ||
            (v.tag == SARN_TAG_BOOL && v.val.bits == 0));
}

void sarn_warn_null_op(const char* file, int line) {
    fprintf(stderr, "[W0024] %s:%d - arithmetic on null; result is null\n", file, line);
}

#define ARITH_OP(name, op)                                                   \
SarnValue sarn_##name(SarnValue a, SarnValue b) {                            \
    if (a.tag == SARN_TAG_NULL || b.tag == SARN_TAG_NULL) return sarn_null();\
    if (a.tag == SARN_TAG_INT  && b.tag == SARN_TAG_INT)                     \
        return sarn_int(a.val.ival op b.val.ival);                           \
    double fa = (a.tag == SARN_TAG_FLOAT) ? a.val.fval : (double)a.val.ival;\
    double fb = (b.tag == SARN_TAG_FLOAT) ? b.val.fval : (double)b.val.ival;\
    return sarn_float(fa op fb);                                             \
}

ARITH_OP(add, +)
ARITH_OP(sub, -)
ARITH_OP(mul, *)

SarnValue sarn_div(SarnValue a, SarnValue b) {
    if (a.tag == SARN_TAG_NULL || b.tag == SARN_TAG_NULL) return sarn_null();
    double fa = (a.tag == SARN_TAG_FLOAT) ? a.val.fval : (double)a.val.ival;
    double fb = (b.tag == SARN_TAG_FLOAT) ? b.val.fval : (double)b.val.ival;
    return sarn_float(fa / fb);
}

SarnValue sarn_mod(SarnValue a, SarnValue b) {
    if (a.tag == SARN_TAG_NULL || b.tag == SARN_TAG_NULL) return sarn_null();
    if (a.tag == SARN_TAG_INT && b.tag == SARN_TAG_INT)
        return sarn_int(a.val.ival % b.val.ival);
    double fa = (a.tag == SARN_TAG_FLOAT) ? a.val.fval : (double)a.val.ival;
    double fb = (b.tag == SARN_TAG_FLOAT) ? b.val.fval : (double)b.val.ival;
    double r  = fa - (int64_t)(fa / fb) * fb;
    return sarn_float(r);
}

int sarn_equal(SarnValue a, SarnValue b) {
    if (a.tag != b.tag) return 0;
    switch (a.tag) {
        case SARN_TAG_NULL:     return 1;
        case SARN_TAG_BOOL:
        case SARN_TAG_INT:      return a.val.ival == b.val.ival;
        case SARN_TAG_FLOAT:    return a.val.fval == b.val.fval;
          case SARN_TAG_STRING: { if (!a.val.ptr || !b.val.ptr) return a.val.ptr == b.val.ptr; return strcmp((const char*)a.val.ptr, (const char*)b.val.ptr) == 0; }
          default:                return a.val.ptr  == b.val.ptr;
    }
}

void sarn_print_str  (const char* s) { printf("%s\n", s); }
void sarn_print_int  (int64_t i)     { printf("%lld\n", (long long)i); }
void sarn_print_float(double f)      { printf("%g\n", f); }
void sarn_print_bool (int b)         { printf("%s\n", b ? "true" : "false"); }
void sarn_print_null (void)          { printf("-null-\n"); }
void sarn_eprint     (const char* m) { fprintf(stderr, "%s\n", m); }

void sarn_exit(int code) { exit(code); }

int64_t sarn_time_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (int64_t)((c.QuadPart * 1000000000LL) / f.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

SarnString* sarn_str_from_cstr(const char* cstr) {
    SarnString* s = (SarnString*)malloc(sizeof(SarnString));
    if (!s) SARN_PANIC("out of memory");
    int32_t len = (int32_t)strlen(cstr);
    s->data = (char*)malloc((size_t)len + 1);
    if (!s->data) SARN_PANIC("out of memory");
    memcpy(s->data, cstr, (size_t)len + 1);
    s->len  = len;
    s->hash = 0;
    return s;
}

const char* sarn_str_cstr(SarnString* s) { return s ? s->data : ""; }

SarnValue sarn_string_new(const char* data, int32_t len) {
    SarnString* s = (SarnString*)malloc(sizeof(SarnString));
    if (!s) SARN_PANIC("out of memory");
    s->data = (char*)malloc((size_t)len + 1);
    if (!s->data) SARN_PANIC("out of memory");
    memcpy(s->data, data, (size_t)len);
    s->data[len] = '\0';
    s->len  = len;
    s->hash = 0;
    SarnValue v;
    v.tag     = SARN_TAG_STRING;
    v.val.ptr = s;
    return v;
}

double sarn_math_sqrt (double x) { return sqrt(x);  }
double sarn_math_pow  (double b, double e) { return pow(b, e); }
double sarn_math_min  (double a, double b) { return a < b ? a : b; }
double sarn_math_max  (double a, double b) { return a > b ? a : b; }


const char* sarn_num_to_str(double x) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%g", x);
    return buf;
}

const char* sarn_i64_to_str(int64_t x) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)x);
    return buf;
}
