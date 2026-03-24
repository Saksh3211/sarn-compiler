#include "slua_buf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define BUF_MAX 256

typedef struct { uint8_t* data; int32_t size; } SluaBuf;
static SluaBuf _bufs[BUF_MAX];
static int     _binit;
static void _b_init(void) { if(!_binit){ memset(_bufs,0,sizeof(_bufs)); _binit=1; } }
static int _bslot(void) { for(int i=0;i<BUF_MAX;i++) if(!_bufs[i].data) return i; return -1; }
int64_t slua_buf_new(int32_t size) {
    _b_init(); int s=_bslot(); if(s<0||size<=0) return -1;
    _bufs[s].data=(uint8_t*)calloc(1,(size_t)size); _bufs[s].size=size;
    return (int64_t)s;
}

int64_t slua_buf_from_str(const char* str, int32_t len) {
    _b_init(); int s=_bslot(); if(s<0||len<=0) return -1;
    _bufs[s].data=(uint8_t*)malloc((size_t)len); _bufs[s].size=len;
    memcpy(_bufs[s].data,str,(size_t)len); return (int64_t)s;
}
int32_t slua_buf_free(int64_t id) {
    if(id<0||id>=BUF_MAX||!_bufs[id].data) return 0;
    free(_bufs[id].data); _bufs[id].data=NULL; _bufs[id].size=0; return 1;
}

int32_t slua_buf_size(int64_t id) { return (id>=0&&id<BUF_MAX&&_bufs[id].data)?_bufs[id].size:0; }
#define BV(id,off) (_bufs[id].data+(off))
#define BC(id,off,n) (id>=0&&id<BUF_MAX&&_bufs[id].data&&(off)>=0&&(off)+(n)<=(int32_t)_bufs[id].size)
int32_t slua_buf_write_u8(int64_t id,int32_t off,int32_t v) { if(!BC(id,off,1)) return 0; *BV(id,off)=(uint8_t)v; return 1; }
int32_t slua_buf_write_u16(int64_t id,int32_t off,int32_t v) { if(!BC(id,off,2)) return 0; memcpy(BV(id,off),&v,2); return 1; }
int32_t slua_buf_write_u32(int64_t id,int32_t off,int32_t v) { if(!BC(id,off,4)) return 0; memcpy(BV(id,off),&v,4); return 1; }
int32_t slua_buf_write_i64(int64_t id,int32_t off,int64_t v) { if(!BC(id,off,8)) return 0; memcpy(BV(id,off),&v,8); return 1; }
int32_t slua_buf_write_f32(int64_t id,int32_t off,double v) { if(!BC(id,off,4)) return 0; float f=(float)v; memcpy(BV(id,off),&f,4); return 1; }
int32_t slua_buf_write_f64(int64_t id,int32_t off,double v) { if(!BC(id,off,8)) return 0; memcpy(BV(id,off),&v,8); return 1; }
int32_t slua_buf_read_u8(int64_t id,int32_t off)  { return BC(id,off,1)?(int32_t)*BV(id,off):0; }
int32_t slua_buf_read_u16(int64_t id,int32_t off) { uint16_t v=0; if(BC(id,off,2)) memcpy(&v,BV(id,off),2); return (int32_t)v; }
int32_t slua_buf_read_u32_i(int64_t id,int32_t off){ uint32_t v=0; if(BC(id,off,4)) memcpy(&v,BV(id,off),4); return (int32_t)v; }
int64_t slua_buf_read_i64(int64_t id,int32_t off) { int64_t v=0; if(BC(id,off,8)) memcpy(&v,BV(id,off),8); return v; }
double  slua_buf_read_f32(int64_t id,int32_t off) { float v=0; if(BC(id,off,4)) memcpy(&v,BV(id,off),4); return (double)v; }
double  slua_buf_read_f64(int64_t id,int32_t off) { double v=0; if(BC(id,off,8)) memcpy(&v,BV(id,off),8); return v; }
char* slua_buf_to_str(int64_t id) {
    if(id<0||id>=BUF_MAX||!_bufs[id].data) return strdup("");
    char* r=(char*)malloc((size_t)_bufs[id].size+1);
    memcpy(r,_bufs[id].data,(size_t)_bufs[id].size); r[_bufs[id].size]='\0'; return r;
}
char* slua_buf_to_hex(int64_t id) {
    if(id<0||id>=BUF_MAX||!_bufs[id].data) return strdup("");
    char* r=(char*)malloc((size_t)_bufs[id].size*2+1);
    for(int32_t i=0;i<_bufs[id].size;i++) sprintf(r+i*2,"%02x",_bufs[id].data[i]);
    r[_bufs[id].size*2]='\0'; return r;
}
int32_t slua_buf_copy(int64_t dst,int32_t doff,int64_t src,int32_t soff,int32_t len) {
    if(!BC(dst,doff,len)||!BC(src,soff,len)) return 0;
    memcpy(BV(dst,doff),BV(src,soff),(size_t)len); return 1;
}
int32_t slua_buf_fill(int64_t id,int32_t off,int32_t len,int32_t val) {
    if(!BC(id,off,len)) return 0; memset(BV(id,off),(uint8_t)val,(size_t)len); return 1;
}
int32_t slua_buf_write_str(int64_t id,int32_t off,const char* s) {
    if(id<0||id>=BUF_MAX||!_bufs[id].data||!s) return 0;
    size_t n=strlen(s); if(off+(int32_t)n>(int32_t)_bufs[id].size) return 0;
    memcpy(BV(id,off),s,n); return (int32_t)n;
}
