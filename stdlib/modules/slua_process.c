#include "slua_process.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int64_t slua_process_run(const char* cmd) { return (int64_t)system(cmd); }
char* slua_process_output(const char* cmd) {
    FILE* f;
#ifdef _WIN32
    f = _popen(cmd, "r");
#else
    f = popen(cmd, "r");
#endif
    if (!f) return strdup("");
    char* r = (char*)malloc(65536); if (!r) { 
#ifdef _WIN32
        _pclose(f);
#else
        pclose(f);
#endif
        return strdup(""); }
    size_t pos=0; char buf[1024];
    while (fgets(buf,sizeof(buf),f) && pos+strlen(buf)<65535) { size_t l=strlen(buf); memcpy(r+pos,buf,l); pos+=l; }
    r[pos]='\0';
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    return r;
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PROC_MAX 64
static HANDLE _procs[PROC_MAX];
static int    _pinit;
static void _p_init(void) { if (!_pinit) { memset(_procs,0,sizeof(_procs)); _pinit=1; } }
int64_t slua_process_spawn(const char* cmd) {
    _p_init(); STARTUPINFOA si={0}; PROCESS_INFORMATION pi={0}; si.cb=sizeof(si);
    char* c=strdup(cmd);
    if (!CreateProcessA(NULL,c,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)) { free(c); return -1; }
    free(c); CloseHandle(pi.hThread);
    for (int i=0;i<PROC_MAX;i++) if(!_procs[i]){ _procs[i]=pi.hProcess; return (int64_t)i; }
    CloseHandle(pi.hProcess); return -1;
}
int32_t slua_process_wait(int64_t id) {
    if (id<0||id>=PROC_MAX||!_procs[id]) return -1;
    DWORD code=0; WaitForSingleObject(_procs[id],INFINITE);
    GetExitCodeProcess(_procs[id],&code); CloseHandle(_procs[id]); _procs[id]=NULL; return (int32_t)code;
}
int32_t slua_process_kill(int64_t id) {
    if (id<0||id>=PROC_MAX||!_procs[id]) return 0;
    int32_t ok=TerminateProcess(_procs[id],1)?1:0; CloseHandle(_procs[id]); _procs[id]=NULL; return ok;
}
int32_t slua_process_alive(int64_t id) {
    if (id<0||id>=PROC_MAX||!_procs[id]) return 0;
    DWORD code; GetExitCodeProcess(_procs[id],&code); return (code==STILL_ACTIVE)?1:0;
}
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#define PROC_MAX 64
static pid_t _pids[PROC_MAX];
static int   _pinit;
static void _p_init(void) { if (!_pinit) { memset(_pids,0,sizeof(_pids)); _pinit=1; } }
int64_t slua_process_spawn(const char* cmd) {
    _p_init(); pid_t pid=fork(); if (!pid) { execl("/bin/sh","sh","-c",cmd,NULL); exit(127); }
    if (pid<0) return -1;
    for (int i=0;i<PROC_MAX;i++) if(!_pids[i]){ _pids[i]=pid; return (int64_t)i; }
    return -1;
}
int32_t slua_process_wait(int64_t id) {
    if (id<0||id>=PROC_MAX||!_pids[id]) return -1;
    int st; waitpid(_pids[id],&st,0); _pids[id]=0;
    return WIFEXITED(st)?WEXITSTATUS(st):-1;
}
int32_t slua_process_kill(int64_t id) {
    if (id<0||id>=PROC_MAX||!_pids[id]) return 0;
    int r=(kill(_pids[id],SIGTERM)==0)?1:0; _pids[id]=0; return r;
}
int32_t slua_process_alive(int64_t id) {
    if (id<0||id>=PROC_MAX||!_pids[id]) return 0;
    return (kill(_pids[id],0)==0)?1:0;
}
#endif
