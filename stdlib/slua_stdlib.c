#include "slua_stdlib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif


/* ── Math ───────────────────────────────────────────────────────────────────── */

double slua_sqrt(double x) { return sqrt(x); }
double slua_pow(double b, double e) { return pow(b, e); }
double slua_sin(double x) { return sin(x); }
double slua_cos(double x) { return cos(x); }
double slua_tan(double x) { return tan(x); }
double slua_log(double x) { return log(x); }
double slua_log2(double x) { return log2(x); }
double slua_exp(double x) { return exp(x); }
double slua_inf(void) { return HUGE_VAL; }
double slua_nan(void) { return nan(""); }
double slua_pi(void) { return 3.14159265358979323846; }
double slua_e(void) { return 2.71828182845904523536; }


/* ── String utils ───────────────────────────────────────────────────────── */

int32_t slua_str_len(const char* s) {
    return s ? (int32_t)strlen(s) : 0;
}

int32_t slua_str_byte(const char* s, int32_t i) {
    if (!s) return 0;

    size_t len = strlen(s);
    if ((size_t)i >= len) return 0;

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

    size_t len = strlen(s);

    if (from < 0) from = 0;
    if (to >= (int32_t)len) to = (int32_t)len - 1;
    if (from > to) return strdup("");

    size_t sz = (size_t)(to - from + 1);

    char* buf = (char*)malloc(sz + 1);
    if (!buf) return NULL;

    memcpy(buf, s + from, sz);
    buf[sz] = '\0';

    return buf;
}

char* slua_int_to_str(int64_t n) {
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
    return s ? (int64_t)strtoll(s, NULL, 10) : 0;
}

double slua_str_to_float(const char* s) {
    return s ? strtod(s, NULL) : 0.0;
}

char* slua_str_upper(const char* s) {
    if (!s) return NULL;

    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;

    for (size_t i = 0; i < len; ++i)
        buf[i] = (char)toupper((unsigned char)s[i]);

    buf[len] = '\0';
    return buf;
}

char* slua_str_lower(const char* s) {
    if (!s) return NULL;

    size_t len = strlen(s);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;

    for (size_t i = 0; i < len; ++i)
        buf[i] = (char)tolower((unsigned char)s[i]);

    buf[len] = '\0';
    return buf;
}

int32_t slua_str_find(const char* haystack, const char* needle, int32_t from) {
    if (!haystack || !needle) return -1;

    size_t hlen = strlen(haystack);

    if (from < 0) from = 0;
    if ((size_t)from >= hlen) return -1;

    const char* found = strstr(haystack + from, needle);
    return found ? (int32_t)(found - haystack) : -1;
}

char* slua_str_trim(const char* s) {
    if (!s) return NULL;

    while (*s && isspace((unsigned char)*s)) s++;

    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1])) len--;

    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;

    memcpy(buf, s, len);
    buf[len] = '\0';

    return buf;
}

char* slua_str_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";

    size_t la = strlen(a);
    size_t lb = strlen(b);

    char* buf = (char*)malloc(la + lb + 1);
    if (!buf) return NULL;

    memcpy(buf, a, la);
    memcpy(buf + la, b, lb);
    buf[la + lb] = '\0';

    return buf;
}


/* ── I/O ────────────────────────────────────────────────────────────────────── */

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
    char tmp[4096];

    if (!fgets(tmp, sizeof(tmp), stdin))
        return strdup("");

    size_t len = strlen(tmp);

    if (len && tmp[len - 1] == '\n') tmp[--len] = '\0';
    if (len && tmp[len - 1] == '\r') tmp[--len] = '\0';

    return strdup(tmp);
}

int32_t slua_read_char(void) {
    int c = fgetc(stdin);
    return (c == EOF) ? -1 : c;
}

void slua_io_clear(void) {
#ifdef _WIN32
    system("cls");
#else
    printf("\033[2J\033[H");
#endif
}


/* ── Terminal (part of io) ─────────────────────────────────────────────────────────── */

void slua_io_set_color(const char* color) {
    if (!color) return;

    if (!strcmp(color,"red")) printf("\033[31m");
    else if (!strcmp(color,"green")) printf("\033[32m");
    else if (!strcmp(color,"yellow")) printf("\033[33m");
    else if (!strcmp(color,"blue")) printf("\033[34m");
    else if (!strcmp(color,"magenta")) printf("\033[35m");
    else if (!strcmp(color,"cyan")) printf("\033[36m");
    else if (!strcmp(color,"white")) printf("\033[37m");
}

void slua_io_reset_color(void) {
    printf("\033[0m");
}

void slua_io_print_color(const char* msg, const char* color) {
    slua_io_set_color(color);

    if (msg)
        printf("%s\n", msg);

    slua_io_reset_color();
}


/* ── Table helpers ─────────────────────────────────────────────────────────── */

static SluaValue box_str(const char* s) {
    SluaValue v;
    memset(&v, 0, sizeof(v));

    v.tag = SLUA_TAG_STRING;
    v.val.ptr = s ? strdup(s) : NULL;

    return v;
}

static SluaValue str_key(const char* s) {
    return box_str(s);
}

SluaTable* slua_tbl_new(void) {
    return slua_table_new();
}

/* setters */

void slua_tbl_iset_i64(SluaTable* t, int64_t key, int64_t val) {
    slua_table_set(t, slua_int(key), slua_int(val));
}

void slua_tbl_iset_f64(SluaTable* t, int64_t key, double val) {
    slua_table_set(t, slua_int(key), slua_float(val));
}

void slua_tbl_iset_str(SluaTable* t, int64_t key, const char* val) {
    slua_table_set(t, slua_int(key), box_str(val));
}

void slua_tbl_iset_bool(SluaTable* t, int64_t key, int32_t val) {
    slua_table_set(t, slua_int(key), slua_bool(val));
}

void slua_tbl_sset_i64(SluaTable* t, const char* key, int64_t val) {
    slua_table_set(t, str_key(key), slua_int(val));
}

void slua_tbl_sset_f64(SluaTable* t, const char* key, double val) {
    slua_table_set(t, str_key(key), slua_float(val));
}

void slua_tbl_sset_str(SluaTable* t, const char* key, const char* val) {
    slua_table_set(t, str_key(key), box_str(val));
}

void slua_tbl_sset_bool(SluaTable* t, const char* key, int32_t val) {
    slua_table_set(t, str_key(key), slua_bool(val));
}


/* getters */

int64_t slua_tbl_iget_i64(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    return (v.tag == SLUA_TAG_FLOAT) ? (int64_t)v.val.fval : v.val.ival;
}

double slua_tbl_iget_f64(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    return (v.tag == SLUA_TAG_INT) ? (double)v.val.ival : v.val.fval;
}

const char* slua_tbl_iget_str(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));

    if (v.tag != SLUA_TAG_STRING || !v.val.ptr)
        return "";

    return (const char*)v.val.ptr;
}

int32_t slua_tbl_iget_bool(SluaTable* t, int64_t key) {
    SluaValue v = slua_table_get(t, slua_int(key));
    return (int32_t)v.val.bits;
}

int64_t slua_tbl_sget_i64(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    return (v.tag == SLUA_TAG_FLOAT) ? (int64_t)v.val.fval : v.val.ival;
}

double slua_tbl_sget_f64(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    return (v.tag == SLUA_TAG_INT) ? (double)v.val.ival : v.val.fval;
}

const char* slua_tbl_sget_str(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));

    if (v.tag != SLUA_TAG_STRING || !v.val.ptr)
        return "";

    return (const char*)v.val.ptr;
}

int32_t slua_tbl_sget_bool(SluaTable* t, const char* key) {
    SluaValue v = slua_table_get(t, str_key(key));
    return (int32_t)v.val.bits;
}


/* ── OS  ───────────────────────────────────────────────────────────── */

int64_t slua_os_time() {
    return (int64_t)time(NULL);
}

void slua_os_sleep(int64_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

void slua_os_sleepS(int64_t s) {
#ifdef _WIN32
    Sleep((DWORD)(s * 1000));
#else
    sleep((unsigned int)s);
#endif
}

char* slua_os_getenv(const char* key) {
    char* v = getenv(key);
    if (!v) return (char*)"";

    char* buf = (char*)malloc(strlen(v) + 1);
    strcpy(buf, v);

    return buf;
}

void slua_os_system(const char* cmd) {
    system(cmd);
}

char* slua_os_cwd() {
    char buf[4096];

#ifdef _WIN32
    _getcwd(buf, sizeof(buf));
#else
    getcwd(buf, sizeof(buf));
#endif

    char* out = (char*)malloc(strlen(buf) + 1);
    strcpy(out, buf);

    return out;
}

char* slua_str_split(const char* s, const char* sep, int32_t index) {
    if (!s || !sep) return strdup("");
    size_t seplen = strlen(sep);
    if (seplen == 0) return strdup(s);
    int32_t count = 0;
    const char* p = s;
    while ((p = strstr(p, sep)) != NULL) { count++; p += seplen; }
    count++;
    if (index < 0 || index >= count) return strdup("");
    p = s;
    for (int32_t i = 0; i < index; i++) {
        const char* next = strstr(p, sep);
        if (!next) return strdup("");
        p = next + seplen;
    }
    const char* end = strstr(p, sep);
    size_t len = end ? (size_t)(end - p) : strlen(p);
    char* out = (char*)malloc(len + 1);
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

int32_t slua_str_count(const char* s, const char* sep) {
    if (!s || !sep || strlen(sep) == 0) return 0;
    int32_t count = 0;
    size_t seplen = strlen(sep);
    const char* p = s;
    while ((p = strstr(p, sep)) != NULL) { count++; p += seplen; }
    return count + 1;
}

int32_t slua_os_is_admin(void) {
#ifdef _WIN32
    BOOL result = FALSE;
    PSID admins = NULL;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&auth, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &admins)) {
        CheckTokenMembership(NULL, admins, &result);
        FreeSid(admins);
    }
    return result ? 1 : 0;
#else
    return (getuid() == 0) ? 1 : 0;
#endif
}

int32_t slua_os_add_to_path(const char* dir) {
#ifdef _WIN32
    HKEY hKey;
    LONG res = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
        0, KEY_READ | KEY_WRITE, &hKey);
    if (res != ERROR_SUCCESS) return 0;
    char cur[32768] = {0};
    DWORD sz = sizeof(cur);
    DWORD type = REG_EXPAND_SZ;
    RegQueryValueExA(hKey, "Path", NULL, &type, (LPBYTE)cur, &sz);
    if (strstr(cur, dir)) { RegCloseKey(hKey); return 1; }
    char newval[32768];
    if (cur[0]) snprintf(newval, sizeof(newval), "%s;%s", cur, dir);
    else        snprintf(newval, sizeof(newval), "%s", dir);
    RegSetValueExA(hKey, "Path", 0, type, (LPBYTE)newval, (DWORD)strlen(newval) + 1);
    RegCloseKey(hKey);
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 3000, NULL);
    return 1;
#else
    return 0;
#endif
}

char* slua_os_get_temp_dir(void) {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    char* r = (char*)malloc(strlen(buf) + 1);
    strcpy(r, buf);
    return r;
#else
    return strdup("/tmp");
#endif
}
