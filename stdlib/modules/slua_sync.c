#include "slua_sync.h"
#include <string.h>
#define SY_MAX 64
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef CRITICAL_SECTION SlMu;
static SlMu _mu[SY_MAX]; static int _used[SY_MAX]; static int _init;
static void _sy_init(void) { if(!_init){ memset(_used,0,sizeof(_used)); _init=1; } }
int64_t slua_sync_mutex_new(void) {
    _sy_init(); for(int i=0;i<SY_MAX;i++) if(!_used[i]){ InitializeCriticalSection(&_mu[i]); _used[i]=1; return (int64_t)i; } return -1;
}
int32_t slua_sync_mutex_lock(int64_t id)    { if(id<0||id>=SY_MAX||!_used[id]) return 0; EnterCriticalSection(&_mu[id]); return 1; }
int32_t slua_sync_mutex_unlock(int64_t id)  { if(id<0||id>=SY_MAX||!_used[id]) return 0; LeaveCriticalSection(&_mu[id]); return 1; }
int32_t slua_sync_mutex_trylock(int64_t id) { if(id<0||id>=SY_MAX||!_used[id]) return 0; return TryEnterCriticalSection(&_mu[id])?1:0; }
int32_t slua_sync_mutex_free(int64_t id)    { if(id<0||id>=SY_MAX||!_used[id]) return 0; DeleteCriticalSection(&_mu[id]); _used[id]=0; return 1; }
#else
#include <pthread.h>
typedef pthread_mutex_t SlMu;
static SlMu _mu[SY_MAX]; static int _used[SY_MAX]; static int _init;
static void _sy_init(void) { if(!_init){ memset(_used,0,sizeof(_used)); _init=1; } }
int64_t slua_sync_mutex_new(void) {
    _sy_init(); for(int i=0;i<SY_MAX;i++) if(!_used[i]){ pthread_mutex_init(&_mu[i],NULL); _used[i]=1; return (int64_t)i; } return -1;
}
int32_t slua_sync_mutex_lock(int64_t id)    { if(id<0||id>=SY_MAX||!_used[id]) return 0; return (pthread_mutex_lock(&_mu[id])==0)?1:0; }
int32_t slua_sync_mutex_unlock(int64_t id)  { if(id<0||id>=SY_MAX||!_used[id]) return 0; return (pthread_mutex_unlock(&_mu[id])==0)?1:0; }
int32_t slua_sync_mutex_trylock(int64_t id) { if(id<0||id>=SY_MAX||!_used[id]) return 0; return (pthread_mutex_trylock(&_mu[id])==0)?1:0; }
int32_t slua_sync_mutex_free(int64_t id)    { if(id<0||id>=SY_MAX||!_used[id]) return 0; pthread_mutex_destroy(&_mu[id]); _used[id]=0; return 1; }
#endif
