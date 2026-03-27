#include "slua_tbl_extra.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int32_t slua_tbl_len_rt(SluaTable* t) { return t ? slua_table_length(t) : 0; }
void slua_tbl_push(SluaTable* t, int64_t val) { if(!t) return; slua_table_insert(t, slua_int(val)); }
void slua_tbl_push_f(SluaTable* t, double val) { if(!t) return; slua_table_insert(t, slua_float(val)); }
void slua_tbl_push_s(SluaTable* t, const char* val) {
    if(!t||!val) return;
    SluaValue v; v.tag=SLUA_TAG_STRING; v.val.ptr=strdup(val);
    slua_table_insert(t, v);
}
void slua_tbl_pop(SluaTable* t) { if(!t||t->array_size<=0) return; slua_table_remove(t,t->array_size); }
int32_t slua_tbl_contains_s(SluaTable* t, const char* val) {
    if(!t||!val) return 0;
    for(int32_t i=1;i<=t->array_size;i++){
        SluaValue v=slua_table_get(t,slua_int((int64_t)i));
        if(v.tag==SLUA_TAG_STRING&&v.val.ptr&&strcmp((const char*)v.val.ptr,val)==0) return 1;
    }
    return 0;
}
int32_t slua_tbl_contains_i(SluaTable* t, int64_t val) {
    if(!t) return 0;
    for(int32_t i=1;i<=t->array_size;i++){
        SluaValue v=slua_table_get(t,slua_int((int64_t)i));
        if((v.tag==SLUA_TAG_INT&&v.val.ival==val)||(v.tag==SLUA_TAG_FLOAT&&(int64_t)v.val.fval==val)) return 1;
    }
    return 0;
}
char* slua_tbl_keys(SluaTable* t) {
    if(!t) return strdup("");
    char* r=(char*)malloc(65536); if(!r) return strdup(""); size_t pos=0;
    if(t->hash_part) {
        for(int i=0;i<t->hash_cap;i++){
            SluaHashNode* n=&t->hash_part[i];
            if(n->key.tag==SLUA_TAG_NULL) continue;
            do {
                char tmp[256];
                if(n->key.tag==SLUA_TAG_STRING&&n->key.val.ptr)
                    snprintf(tmp,sizeof(tmp),"%s",(const char*)n->key.val.ptr);
                else if(n->key.tag==SLUA_TAG_INT)
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
void slua_tbl_remove_at(SluaTable* t, int32_t idx) { if(t) slua_table_remove(t,(int32_t)idx); }
void slua_tbl_clear(SluaTable* t) {
    if(!t) return;
    t->array_size=0;
    if(t->hash_part){ for(int i=0;i<t->hash_cap;i++){ SluaHashNode* c=t->hash_part[i].next; while(c){SluaHashNode* nx=c->next; free(c); c=nx;} t->hash_part[i].key.tag=SLUA_TAG_NULL; t->hash_part[i].next=NULL; } t->hash_count=0; }
}
SluaTable* slua_tbl_merge(SluaTable* a, SluaTable* b) {
    SluaTable* r=slua_table_new();
    if(a) for(int32_t i=1;i<=a->array_size;i++) slua_table_insert(r,slua_table_get(a,slua_int((int64_t)i)));
    if(b) for(int32_t i=1;i<=b->array_size;i++) slua_table_insert(r,slua_table_get(b,slua_int((int64_t)i)));
    return r;
}
SluaTable* slua_tbl_slice(SluaTable* t, int32_t from, int32_t to) {
    SluaTable* r=slua_table_new();
    if(!t) return r;
    if(from<1) from=1; if(to>t->array_size) to=t->array_size;
    for(int32_t i=from;i<=to;i++) slua_table_insert(r,slua_table_get(t,slua_int((int64_t)i)));
    return r;
}
void slua_tbl_reverse(SluaTable* t) {
    if(!t||t->array_size<=1) return;
    for(int32_t i=1,j=t->array_size;i<j;i++,j--){
        SluaValue a=slua_table_get(t,slua_int((int64_t)i));
        SluaValue b=slua_table_get(t,slua_int((int64_t)j));
        slua_table_set(t,slua_int((int64_t)i),b);
        slua_table_set(t,slua_int((int64_t)j),a);
    }
}
