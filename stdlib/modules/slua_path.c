#include "slua_path.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define slua_stat _stat
#define S_IFMT  _S_IFMT
#define S_IFREG _S_IFREG
#define S_IFDIR _S_IFDIR
#else
#include <unistd.h>
#include <limits.h>
#define slua_stat stat
#endif
char* slua_path_join(const char* a, const char* b) {
    size_t la=strlen(a), lb=strlen(b);
    char* r=(char*)malloc(la+lb+3); if(!r) return strdup("");
    memcpy(r, a, la);
    if (la && a[la-1]!='/' && a[la-1]!='\\') r[la++]='/';
    memcpy(r+la, b, lb+1); return r;
}
char* slua_path_basename(const char* path) {
    const char* p=strrchr(path,'/'), *p2=strrchr(path,'\\');
    if (p2>p) p=p2; return strdup(p ? p+1 : path);
}
char* slua_path_dirname(const char* path) {
    const char* p=strrchr(path,'/'), *p2=strrchr(path,'\\');
    if (p2>p) p=p2; if (!p) return strdup(".");
    size_t n=(size_t)(p-path); char* r=(char*)malloc(n+1); memcpy(r,path,n); r[n]='\0'; return r;
}
char* slua_path_extension(const char* path) {
    const char* p=strrchr(path,'/'), *p2=strrchr(path,'\\');
    if (p2>p) p=p2; const char* base=p?p+1:path;
    const char* dot=strrchr(base,'.'); return strdup(dot?dot:"");
}
char* slua_path_stem(const char* path) {
    const char* p=strrchr(path,'/'), *p2=strrchr(path,'\\');
    if (p2>p) p=p2; const char* base=p?p+1:path;
    const char* dot=strrchr(base,'.'); size_t n=dot?(size_t)(dot-base):strlen(base);
    char* r=(char*)malloc(n+1); memcpy(r,base,n); r[n]='\0'; return r;
}
char* slua_path_absolute(const char* path) {
    char* r=(char*)malloc(4096); if (!r) return strdup(path);
#ifdef _WIN32
    if (!GetFullPathNameA(path,4096,r,NULL)) { free(r); return strdup(path); }
#else
    if (!realpath(path,r)) { free(r); return strdup(path); }
#endif
    return r;
}
int32_t slua_path_exists(const char* path)  { struct slua_stat st; return (slua_stat(path,&st)==0)?1:0; }
int32_t slua_path_is_file(const char* path) { struct slua_stat st; return (slua_stat(path,&st)==0&&(st.st_mode&S_IFMT)==S_IFREG)?1:0; }
int32_t slua_path_is_dir(const char* path)  { struct slua_stat st; return (slua_stat(path,&st)==0&&(st.st_mode&S_IFMT)==S_IFDIR)?1:0; }
char* slua_path_normalize(const char* path) { return slua_path_absolute(path); }
