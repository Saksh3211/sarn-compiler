#include "sarn_tbl_extra.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int32_t sarn_tbl_len_rt(SarnTable* t) { return t ? sarn_table_length(t) : 0; }
void sarn_tbl_push(SarnTable* t, int64_t val) { if(!t) return; sarn_table_insert(t, sarn_int(val)); }
void sarn_tbl_push_f(SarnTable* t, double val) { if(!t) return; sarn_table_insert(t, sarn_float(val)); }
void sarn_tbl_push_s(SarnTable* t, const char* val) {
    if(!t||!val) return;
    SarnValue v; v.tag=SARN_TAG_STRING; v.val.ptr=strdup(val);
    sarn_table_insert(t, v);
}
void sarn_tbl_pop(SarnTable* t) { if(!t||t->array_size<=0) return; sarn_table_remove(t,t->array_size); }
int32_t sarn_tbl_contains_s(SarnTable* t, const char* val) {
    if(!t||!val) return 0;
    for(int32_t i=1;i<=t->array_size;i++){
        SarnValue v=sarn_table_get(t,sarn_int((int64_t)i));
        if(v.tag==SARN_TAG_STRING&&v.val.ptr&&strcmp((const char*)v.val.ptr,val)==0) return 1;
    }
    return 0;
}
int32_t sarn_tbl_contains_i(SarnTable* t, int64_t val) {
    if(!t) return 0;
    for(int32_t i=1;i<=t->array_size;i++){
        SarnValue v=sarn_table_get(t,sarn_int((int64_t)i));
        if((v.tag==SARN_TAG_INT&&v.val.ival==val)||(v.tag==SARN_TAG_FLOAT&&(int64_t)v.val.fval==val)) return 1;
    }
    return 0;
}
char* sarn_tbl_keys(SarnTable* t) {
    if(!t) return strdup("");
    char* r=(char*)malloc(65536); if(!r) return strdup(""); size_t pos=0;
    if(t->hash_part) {
        for(int i=0;i<t->hash_cap;i++){
            SarnHashNode* n=&t->hash_part[i];
            if(n->key.tag==SARN_TAG_NULL) continue;
            do {
                char tmp[256];
                if(n->key.tag==SARN_TAG_STRING&&n->key.val.ptr)
                    snprintf(tmp,sizeof(tmp),"%s",(const char*)n->key.val.ptr);
                else if(n->key.tag==SARN_TAG_INT)
                    snprintf(tmp,sizeof(tmp),"%lld",(long long)n->key.val.ival);
                else snprintf(tmp,sizeof(tmp),"?");
                size_t l=strlen(tmp);
                if(pos+l+2<65535){ memcpy(r+pos,tmp,l); pos+=l; r[pos++]='\n'; }
                n=n->next;
            } while(n);
        }
    }
    for(int32_t i=1;i<=t->array_size;i++){
        char tmp[32]; snprintf(tmp,sizeof(tmp),"%d",i); size_t l=strlen(tmp);
        if(pos+l+2<65535){ memcpy(r+pos,tmp,l); pos+=l; r[pos++]='\n'; }
    }
    r[pos]='\0'; return r;
}
void sarn_tbl_remove_at(SarnTable* t, int32_t idx) { if(t) sarn_table_remove(t,(int32_t)idx); }
void sarn_tbl_clear(SarnTable* t) {
    if(!t) return;
    t->array_size=0;
    if(t->hash_part){ for(int i=0;i<t->hash_cap;i++){ SarnHashNode* c=t->hash_part[i].next; while(c){SarnHashNode* nx=c->next; free(c); c=nx;} t->hash_part[i].key.tag=SARN_TAG_NULL; t->hash_part[i].next=NULL; } t->hash_count=0; }
}
SarnTable* sarn_tbl_merge(SarnTable* a, SarnTable* b) {
    SarnTable* r=sarn_table_new();
    if(a) for(int32_t i=1;i<=a->array_size;i++) sarn_table_insert(r,sarn_table_get(a,sarn_int((int64_t)i)));
    if(b) for(int32_t i=1;i<=b->array_size;i++) sarn_table_insert(r,sarn_table_get(b,sarn_int((int64_t)i)));
    return r;
}
SarnTable* sarn_tbl_slice(SarnTable* t, int32_t from, int32_t to) {
    SarnTable* r=sarn_table_new();
    if(!t) return r;
    if(from<1) from=1; if(to>t->array_size) to=t->array_size;
    for(int32_t i=from;i<=to;i++) sarn_table_insert(r,sarn_table_get(t,sarn_int((int64_t)i)));
    return r;
}
void sarn_tbl_reverse(SarnTable* t) {
    if(!t||t->array_size<=1) return;
    for(int32_t i=1,j=t->array_size;i<j;i++,j--){
        SarnValue a=sarn_table_get(t,sarn_int((int64_t)i));
        SarnValue b=sarn_table_get(t,sarn_int((int64_t)j));
        sarn_table_set(t,sarn_int((int64_t)i),b);
        sarn_table_set(t,sarn_int((int64_t)j),a);
    }
}
