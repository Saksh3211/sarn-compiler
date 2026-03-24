#include "slua_thread.h"
#include <string.h>
#include <stdlib.h>
#define TH_MAX 64
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef struct { SluaThreadFn fn; void* arg; } ThCtx;
static HANDLE _ths[TH_MAX]; static int _tused[TH_MAX]; static int _tinit;
static void _t_init(void) { if(!_tinit){ memset(_ths,0,sizeof(_ths)); memset(_tused,0,sizeof(_tused)); _tinit=1; } }
static DWORD WINAPI _th_run(LPVOID p) { ThCtx* c=(ThCtx*)p; c->fn(c->arg); free(c); return 0; }
int64_t slua_thread_create(SluaThreadFn fn, void* arg) {
    _t_init(); for(int i=0;i<TH_MAX;i++) if(!_tused[i]){
        ThCtx* c=(ThCtx*)malloc(sizeof(ThCtx)); c->fn=fn; c->arg=arg;
        _ths[i]=CreateThread(NULL,0,_th_run,c,0,NULL);
        if(!_ths[i]){ free(c); return -1; } _tused[i]=1; return (int64_t)i;
    } return -1;
}
int32_t slua_thread_join(int64_t id) {
    if(id<0||id>=TH_MAX||!_tused[id]) return 0;
    WaitForSingleObject(_ths[id],INFINITE); CloseHandle(_ths[id]); _ths[id]=NULL; _tused[id]=0; return 1;
}
int32_t slua_thread_detach(int64_t id) {
    if(id<0||id>=TH_MAX||!_tused[id]) return 0;
    CloseHandle(_ths[id]); _ths[id]=NULL; _tused[id]=0; return 1;
}
int32_t slua_thread_alive(int64_t id) {
    if(id<0||id>=TH_MAX||!_tused[id]) return 0;
    DWORD code; GetExitCodeThread(_ths[id],&code); return (code==STILL_ACTIVE)?1:0;
}
void    slua_thread_sleep_ms(int64_t ms) { Sleep((DWORD)ms); }
int64_t slua_thread_self_id(void) { return (int64_t)GetCurrentThreadId(); }
#else
#include <pthread.h>
#include <unistd.h>
typedef struct { SluaThreadFn fn; void* arg; } ThCtx;
static pthread_t _ths[TH_MAX]; static int _tused[TH_MAX]; static int _tinit;
static void _t_init(void) { if(!_tinit){ memset(_tused,0,sizeof(_tused)); _tinit=1; } }
static void* _th_run(void* p) { ThCtx* c=(ThCtx*)p; c->fn(c->arg); free(c); return NULL; }
int64_t slua_thread_create(SluaThreadFn fn, void* arg) {
    _t_init(); for(int i=0;i<TH_MAX;i++) if(!_tused[i]){
        ThCtx* c=(ThCtx*)malloc(sizeof(ThCtx)); c->fn=fn; c->arg=arg;
        if(pthread_create(&_ths[i],NULL,_th_run,c)!=0){ free(c); return -1; }
        _tused[i]=1; return (int64_t)i;
    } return -1;
}
int32_t slua_thread_join(int64_t id) {
    if(id<0||id>=TH_MAX||!_tused[id]) return 0;
    pthread_join(_ths[id],NULL); _tused[id]=0; return 1;
}
int32_t slua_thread_detach(int64_t id) {
    if(id<0||id>=TH_MAX||!_tused[id]) return 0;
    pthread_detach(_ths[id]); _tused[id]=0; return 1;
}
int32_t slua_thread_alive(int64_t id) {
    if(id<0||id>=TH_MAX||!_tused[id]) return 0;
    return (pthread_kill(_ths[id],0)==0)?1:0;
}
void    slua_thread_sleep_ms(int64_t ms) { usleep((useconds_t)(ms*1000)); }
int64_t slua_thread_self_id(void) { return (int64_t)(uintptr_t)pthread_self(); }
#endif
