#include "../include/slua_runtime.h"
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

SluaAllocator slua_sys_allocator = {
    .alloc_fn = sys_alloc_fn,
    .free_fn  = sys_free_fn,
    .ctx      = NULL,
};

void* slua_alloc       (size_t bytes)              { return malloc(bytes); }
void  slua_free        (void* ptr)                 { free(ptr); }
void* slua_alloc_zeroed(size_t bytes)              { return calloc(1, bytes); }
void* slua_realloc     (void* ptr, size_t new_size){ return realloc(ptr, new_size); }

void slua_panic(const char* msg, const char* file, int line) {
    fprintf(stderr, "\n[PANIC] %s\n  at %s:%d\n", msg, file, line);
    abort();
}

void slua_print_value(SluaValue v) {
    switch (v.tag) {
        case SLUA_TAG_NULL:     printf("-null-"); break;
        case SLUA_TAG_BOOL:     printf("%s", v.val.bits ? "true" : "false"); break;
        case SLUA_TAG_INT:      printf("%lld", (long long)v.val.ival); break;
        case SLUA_TAG_FLOAT:    printf("%g", v.val.fval); break;
        case SLUA_TAG_STRING: {
            SluaString* s = (SluaString*)v.val.ptr;
            if (s && s->data) fwrite(s->data, 1, (size_t)s->len, stdout);
            break;
        }
        case SLUA_TAG_TABLE:    printf("<table:%p>",    v.val.ptr); break;
        case SLUA_TAG_FUNCTION: printf("<function:%p>", v.val.ptr); break;
        case SLUA_TAG_PTR:      printf("<ptr:%p>",      v.val.ptr); break;
        default:                printf("<unknown>"); break;
    }
}

const char* slua_typename(SluaValue v) {
    switch (v.tag) {
        case SLUA_TAG_NULL:     return "null";
        case SLUA_TAG_BOOL:     return "bool";
        case SLUA_TAG_INT:      return "int";
        case SLUA_TAG_FLOAT:    return "number";
        case SLUA_TAG_STRING:   return "string";
        case SLUA_TAG_TABLE:    return "table";
        case SLUA_TAG_FUNCTION: return "function";
        case SLUA_TAG_PTR:      return "ptr";
        default:                return "unknown";
    }
}

int slua_is_null(SluaValue v) { return v.tag == SLUA_TAG_NULL; }
int slua_truthy (SluaValue v) {
    return !(v.tag == SLUA_TAG_NULL ||
            (v.tag == SLUA_TAG_BOOL && v.val.bits == 0));
}

void slua_warn_null_op(const char* file, int line) {
    fprintf(stderr, "[W0024] %s:%d - arithmetic on null; result is null\n", file, line);
}

#define ARITH_OP(name, op)                                                   \
SluaValue slua_##name(SluaValue a, SluaValue b) {                            \
    if (a.tag == SLUA_TAG_NULL || b.tag == SLUA_TAG_NULL) return slua_null();\
    if (a.tag == SLUA_TAG_INT  && b.tag == SLUA_TAG_INT)                     \
        return slua_int(a.val.ival op b.val.ival);                           \
    double fa = (a.tag == SLUA_TAG_FLOAT) ? a.val.fval : (double)a.val.ival;\
    double fb = (b.tag == SLUA_TAG_FLOAT) ? b.val.fval : (double)b.val.ival;\
    return slua_float(fa op fb);                                             \
}

ARITH_OP(add, +)
ARITH_OP(sub, -)
ARITH_OP(mul, *)

SluaValue slua_div(SluaValue a, SluaValue b) {
    if (a.tag == SLUA_TAG_NULL || b.tag == SLUA_TAG_NULL) return slua_null();
    double fa = (a.tag == SLUA_TAG_FLOAT) ? a.val.fval : (double)a.val.ival;
    double fb = (b.tag == SLUA_TAG_FLOAT) ? b.val.fval : (double)b.val.ival;
    return slua_float(fa / fb);
}

SluaValue slua_mod(SluaValue a, SluaValue b) {
    if (a.tag == SLUA_TAG_NULL || b.tag == SLUA_TAG_NULL) return slua_null();
    if (a.tag == SLUA_TAG_INT && b.tag == SLUA_TAG_INT)
        return slua_int(a.val.ival % b.val.ival);
    double fa = (a.tag == SLUA_TAG_FLOAT) ? a.val.fval : (double)a.val.ival;
    double fb = (b.tag == SLUA_TAG_FLOAT) ? b.val.fval : (double)b.val.ival;
    double r  = fa - (int64_t)(fa / fb) * fb;
    return slua_float(r);
}

int slua_equal(SluaValue a, SluaValue b) {
    if (a.tag != b.tag) return 0;
    switch (a.tag) {
        case SLUA_TAG_NULL:     return 1;
        case SLUA_TAG_BOOL:
        case SLUA_TAG_INT:      return a.val.ival == b.val.ival;
        case SLUA_TAG_FLOAT:    return a.val.fval == b.val.fval;
        default:                return a.val.ptr  == b.val.ptr;
    }
}

void slua_print_str  (const char* s) { printf("%s\n", s); }
void slua_print_int  (int64_t i)     { printf("%lld\n", (long long)i); }
void slua_print_float(double f)      { printf("%g\n", f); }
void slua_print_bool (int b)         { printf("%s\n", b ? "true" : "false"); }
void slua_print_null (void)          { printf("-null-\n"); }
void slua_eprint     (const char* m) { fprintf(stderr, "%s\n", m); }

void slua_exit(int code) { exit(code); }

int64_t slua_time_ns(void) {
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

SluaString* slua_str_from_cstr(const char* cstr) {
    SluaString* s = (SluaString*)malloc(sizeof(SluaString));
    if (!s) SLUA_PANIC("out of memory");
    int32_t len = (int32_t)strlen(cstr);
    s->data = (char*)malloc((size_t)len + 1);
    if (!s->data) SLUA_PANIC("out of memory");
    memcpy(s->data, cstr, (size_t)len + 1);
    s->len  = len;
    s->hash = 0;
    return s;
}

const char* slua_str_cstr(SluaString* s) { return s ? s->data : ""; }

SluaValue slua_string_new(const char* data, int32_t len) {
    SluaString* s = (SluaString*)malloc(sizeof(SluaString));
    if (!s) SLUA_PANIC("out of memory");
    s->data = (char*)malloc((size_t)len + 1);
    if (!s->data) SLUA_PANIC("out of memory");
    memcpy(s->data, data, (size_t)len);
    s->data[len] = '\0';
    s->len  = len;
    s->hash = 0;
    SluaValue v;
    v.tag     = SLUA_TAG_STRING;
    v.val.ptr = s;
    return v;
}

double slua_math_floor(double x) { return floor(x); }
double slua_math_ceil (double x) { return ceil(x);  }
double slua_math_sqrt (double x) { return sqrt(x);  }
double slua_math_abs  (double x) { return fabs(x);  }
double slua_math_pow  (double b, double e) { return pow(b, e); }
double slua_math_min  (double a, double b) { return a < b ? a : b; }
double slua_math_max  (double a, double b) { return a > b ? a : b; }


const char* slua_num_to_str(double x) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%g", x);
    return buf;
}

const char* slua_i64_to_str(int64_t x) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)x);
    return buf;
}