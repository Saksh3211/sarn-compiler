#include "slua_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define slua_stat  _stat
#define slua_mkdir(p) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define slua_stat  stat
#define slua_mkdir(p) mkdir(p, 0755)
#endif
#define FS_MAX 64
static FILE* _h[FS_MAX];
static int   _hinit;
static void _fs_init(void) { if (!_hinit) { memset(_h, 0, sizeof(_h)); _hinit = 1; } }
char* slua_fs_read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char* b = (char*)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return strdup(""); }
    fread(b, 1, (size_t)sz, f); b[sz] = '\0'; fclose(f); return b;
}
int32_t slua_fs_write(const char* path, const char* data) {
    FILE* f = fopen(path, "wb"); if (!f) return 0;
    size_t n = strlen(data); int32_t ok = (int32_t)(fwrite(data, 1, n, f) == n);
    fclose(f); return ok;
}
int32_t slua_fs_append(const char* path, const char* data) {
    FILE* f = fopen(path, "ab"); if (!f) return 0;
    size_t n = strlen(data); int32_t ok = (int32_t)(fwrite(data, 1, n, f) == n);
    fclose(f); return ok;
}
int32_t slua_fs_exists(const char* path) { struct slua_stat st; return (slua_stat(path, &st) == 0) ? 1 : 0; }
int32_t slua_fs_delete(const char* path) { return (remove(path) == 0) ? 1 : 0; }
int32_t slua_fs_mkdir(const char* path)  { return (slua_mkdir(path) == 0) ? 1 : 0; }
int32_t slua_fs_rename(const char* a, const char* b) { return (rename(a, b) == 0) ? 1 : 0; }
int64_t slua_fs_size(const char* path) { struct slua_stat st; return (slua_stat(path, &st) == 0) ? (int64_t)st.st_size : -1; }
char* slua_fs_listdir(const char* path) {
    char* r = (char*)calloc(1, 65536); if (!r) return strdup(""); size_t pos = 0;
#ifdef _WIN32
    char pat[4096]; snprintf(pat, sizeof(pat), "%s\\*", path);
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        if (fd.cFileName[0] == '.' && (!fd.cFileName[1] || (fd.cFileName[1]=='.'&&!fd.cFileName[2]))) continue;
        size_t l = strlen(fd.cFileName);
        if (pos + l + 2 < 65536) { memcpy(r+pos, fd.cFileName, l); pos += l; r[pos++] = '\n'; }
    } while (FindNextFileA(h, &fd)); FindClose(h);
#else
    DIR* d = opendir(path); if (!d) return r; struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue; size_t l = strlen(e->d_name);
        if (pos+l+2 < 65536) { memcpy(r+pos, e->d_name, l); pos+=l; r[pos++]='\n'; }
    } closedir(d);
#endif
    return r;
}
int64_t slua_fs_open(const char* path, const char* mode) {
    _fs_init();
    for (int i = 0; i < FS_MAX; i++) if (!_h[i]) { _h[i] = fopen(path, mode); return _h[i] ? (int64_t)i : -1; }
    return -1;
}
int32_t slua_fs_close(int64_t h) {
    if (h<0||h>=FS_MAX||!_h[h]) return 0; fclose(_h[h]); _h[h]=NULL; return 1;
}
char* slua_fs_readline(int64_t h) {
    if (h<0||h>=FS_MAX||!_h[h]) return strdup("");
    char tmp[4096]; if (!fgets(tmp,sizeof(tmp),_h[h])) return strdup("");
    size_t n=strlen(tmp); if(n&&tmp[n-1]=='\n') tmp[--n]='\0'; if(n&&tmp[n-1]=='\r') tmp[--n]='\0';
    return strdup(tmp);
}
int32_t slua_fs_writeh(int64_t h, const char* data) {
    if (h<0||h>=FS_MAX||!_h[h]) return 0; size_t n=strlen(data); return (int32_t)(fwrite(data,1,n,_h[h])==n);
}
int32_t slua_fs_flush(int64_t h) { if(h<0||h>=FS_MAX||!_h[h]) return 0; return (fflush(_h[h])==0)?1:0; }
int32_t slua_fs_copy(const char* src, const char* dst) {
#ifdef _WIN32
    return CopyFileA(src, dst, FALSE) ? 1 : 0;
#else
    char* d = slua_fs_read_all(src); if (!d) return 0; int32_t ok = slua_fs_write(dst, d); free(d); return ok;
#endif
}
