#include "slua_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static char* json_esc(const char* s) {
    size_t n=strlen(s); char* r=(char*)malloc(n*2+3); if(!r) return strdup("\"\"");
    size_t p=0; r[p++]='"';
    for (size_t i=0;i<n;i++) {
        char c=s[i];
        if (c=='"')       { r[p++]='\\'; r[p++]='"'; }
        else if (c=='\\') { r[p++]='\\'; r[p++]='\\'; }
        else if (c=='\n') { r[p++]='\\'; r[p++]='n'; }
        else if (c=='\r') { r[p++]='\\'; r[p++]='r'; }
        else if (c=='\t') { r[p++]='\\'; r[p++]='t'; }
        else               r[p++]=c;
    }
    r[p++]='"'; r[p]='\0'; return r;
}
char* slua_json_encode_str(const char* s)   { return json_esc(s?s:""); }
char* slua_json_encode_int(int64_t n)       { char* r=(char*)malloc(32); snprintf(r,32,"%lld",(long long)n); return r; }
char* slua_json_encode_float(double f)      { char* r=(char*)malloc(64); snprintf(r,64,"%g",f); return r; }
char* slua_json_encode_bool(int32_t b)      { return strdup(b?"true":"false"); }
char* slua_json_encode_null(void)           { return strdup("null"); }
static const char* _find_key(const char* json, const char* key) {
    char pat[512]; snprintf(pat,sizeof(pat),"\"%s\"",key);
    const char* p=json;
    while ((p=strstr(p,pat))!=NULL) {
        p+=strlen(pat);
        while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
        if (*p==':') { p++; while (*p==' ') p++; return p; }
    }
    return NULL;
}
char* slua_json_get_str(const char* json, const char* key) {
    const char* v=_find_key(json,key); if (!v||*v!='"') return strdup("");
    v++;
    const char* e=v;
    while (*e) { if (*e=='"') break; if (*e=='\\') { e++; if(*e) e++; continue; } e++; }
    size_t n=(size_t)(e-v); char* r=(char*)malloc(n+1); memcpy(r,v,n); r[n]='\0'; return r;
}
int64_t slua_json_get_int(const char* json, const char* key)   { const char* v=_find_key(json,key); return v?(int64_t)strtoll(v,NULL,10):0; }
double  slua_json_get_float(const char* json, const char* key) { const char* v=_find_key(json,key); return v?strtod(v,NULL):0.0; }
int32_t slua_json_get_bool(const char* json, const char* key)  { const char* v=_find_key(json,key); return v?(*v=='t'):0; }
int32_t slua_json_has_key(const char* json, const char* key)   { return _find_key(json,key)?1:0; }
char*   slua_json_minify(const char* json)                     { return strdup(json?json:""); }
char* slua_json_get_array_item(const char* json, const char* key, int32_t index) {
    const char* v=_find_key(json,key); if (!v||*v!='[') return strdup("");
    v++; int ci=0;
    while (*v&&ci<index) { if (*v==',') ci++; v++; }
    while (*v==' '||*v=='\n'||*v=='\t') v++;
    if (!*v||*v==']') return strdup("");
    if (*v=='"') {
        v++; const char* e=v;
        while (*e&&*e!='"') e++; size_t n=(size_t)(e-v);
        char* r=(char*)malloc(n+1); memcpy(r,v,n); r[n]='\0'; return r;
    }
    const char* e=v; while (*e&&*e!=','&&*e!=']') e++;
    size_t n=(size_t)(e-v); char* r=(char*)malloc(n+1); memcpy(r,v,n); r[n]='\0'; return r;
}

double slua_json_get_nested_float(const char* json, const char* outer, const char* inner) {
    const char* pat1 = outer;
    char needle[512]; snprintf(needle, sizeof(needle), "\"%s\"", pat1);
    const char* p = strstr(json, needle);
    if (!p) return 0.0;
    p += strlen(needle);
    while (*p==' '||*p=='\t'||*p=='\n') p++;
    if (*p != ':') return 0.0; p++;
    while (*p==' ') p++;
    if (*p != '{') return 0.0;
    char sub[4096]; size_t depth=0, i=0;
    do { if(*p=='{') depth++; else if(*p=='}') depth--; if(i<4095) sub[i++]=*p; p++; } while(*p && depth>0);
    sub[i]='\0';
    char needle2[512]; snprintf(needle2, sizeof(needle2), "\"%s\"", inner);
    const char* q = strstr(sub, needle2);
    if (!q) return 0.0;
    q += strlen(needle2);
    while (*q==' ') q++;
    if (*q != ':') return 0.0; q++;
    while (*q==' ') q++;
    return strtod(q, NULL);
}

int64_t slua_json_get_nested_int(const char* json, const char* outer, const char* inner) {
    return (int64_t)slua_json_get_nested_float(json, outer, inner);
}

char* slua_json_get_nested_str(const char* json, const char* outer, const char* inner) {
    const char* pat1 = outer;
    char needle[512]; snprintf(needle, sizeof(needle), "\"%s\"", pat1);
    const char* p = strstr(json, needle);
    if (!p) return strdup("");
    p += strlen(needle);
    while (*p==' '||*p=='\t'||*p=='\n') p++;
    if (*p != ':') return strdup(""); p++;
    while (*p==' ') p++;
    if (*p != '{') return strdup("");
    char sub[4096]; size_t depth=0, i=0;
    do { if(*p=='{') depth++; else if(*p=='}') depth--; if(i<4095) sub[i++]=*p; p++; } while(*p && depth>0);
    sub[i]='\0';
    char needle2[512]; snprintf(needle2, sizeof(needle2), "\"%s\"", inner);
    const char* q = strstr(sub, needle2);
    if (!q) return strdup("");
    q += strlen(needle2);
    while (*q==' ') q++;
    if (*q != ':') return strdup(""); q++;
    while (*q==' ') q++;
    if (*q != '"') return strdup(""); q++;
    const char* e = q; while(*e && *e!='"') e++;
    size_t n=(size_t)(e-q); char* r=(char*)malloc(n+1); memcpy(r,q,n); r[n]='\0'; return r;
}
