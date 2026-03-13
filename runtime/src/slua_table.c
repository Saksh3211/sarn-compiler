#include "../include/slua_runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TABLE_INIT_ARRAY_CAP 8
#define TABLE_INIT_HASH_CAP  8

SluaTable* slua_table_new(void) {
    SluaTable* t = (SluaTable*)calloc(1, sizeof(SluaTable));
    if (!t) SLUA_PANIC("out of memory: slua_table_new");
    t->array_part = (SluaValue*)calloc(TABLE_INIT_ARRAY_CAP, sizeof(SluaValue));
    t->array_cap  = TABLE_INIT_ARRAY_CAP;
    t->array_size = 0;
    return t;
}

void slua_table_free(SluaTable* t) {
    if (!t) return;
    free(t->array_part);
    if (t->hash_part) {
        for (int i = 0; i < t->hash_cap; i++) {
            SluaHashNode* chain = t->hash_part[i].next;
            while (chain) {
                SluaHashNode* tmp = chain->next;
                free(chain);
                chain = tmp;
            }
        }
        free(t->hash_part);
    }
    free(t);
}

static int is_array_key(SluaValue key, int32_t cap) {
    return key.tag == SLUA_TAG_INT && key.val.ival >= 1 && key.val.ival <= cap;
}

static uint32_t hash_key(SluaValue key) {
    switch (key.tag) {
        case SLUA_TAG_INT:    return (uint32_t)(key.val.ival ^ (key.val.ival >> 32));
        case SLUA_TAG_FLOAT:  { uint64_t b; memcpy(&b, &key.val.fval, 8); return (uint32_t)(b ^ (b >> 32)); }
        case SLUA_TAG_BOOL:   return (uint32_t)key.val.bits;
        case SLUA_TAG_STRING: {
            const char* cs = (const char*)key.val.ptr;
            if (!cs) return 0;
            uint32_t h = 2166136261u;
            while (*cs) h = (h ^ (uint8_t)*cs++) * 16777619u;
            return h;
        }
        default: return (uint32_t)(uintptr_t)key.val.ptr;
    }
}

static int slua_key_equal(SluaValue a, SluaValue b) {
    if (a.tag != b.tag) return 0;
    if (a.tag == SLUA_TAG_STRING) {
        if (!a.val.ptr || !b.val.ptr) return a.val.ptr == b.val.ptr;
        return strcmp((const char*)a.val.ptr, (const char*)b.val.ptr) == 0;
    }
    return a.val.ival == b.val.ival;
}

SluaValue slua_table_get(SluaTable* t, SluaValue key) {
    if (!t) return slua_null();
    if (is_array_key(key, t->array_size))
        return t->array_part[key.val.ival - 1];
    if (!t->hash_part || t->hash_cap == 0) return slua_null();
    uint32_t idx = hash_key(key) % (uint32_t)t->hash_cap;
    SluaHashNode* node = &t->hash_part[idx];
    if (node->key.tag == SLUA_TAG_NULL) return slua_null();
    do {
        if (slua_key_equal(node->key, key)) return node->val;
        node = node->next;
    } while (node);
    return slua_null();
}

void slua_table_set(SluaTable* t, SluaValue key, SluaValue val) {
    if (!t) return;
    if (key.tag == SLUA_TAG_INT) {
        int64_t idx = key.val.ival;
        if (idx >= 1 && idx <= (int64_t)t->array_size + 1) {
            if (idx > t->array_cap) {
                int32_t new_cap = t->array_cap * 2;
                while (new_cap < idx) new_cap *= 2;
                t->array_part = (SluaValue*)realloc(t->array_part, sizeof(SluaValue) * (size_t)new_cap);
                if (!t->array_part) SLUA_PANIC("out of memory: table resize");
                memset(t->array_part + t->array_cap, 0, sizeof(SluaValue) * (size_t)(new_cap - t->array_cap));
                t->array_cap = new_cap;
            }
            t->array_part[idx - 1] = val;
            if (idx == t->array_size + 1) t->array_size++;
            return;
        }
    }
    if (!t->hash_part) {
        t->hash_part = (SluaHashNode*)calloc((size_t)TABLE_INIT_HASH_CAP, sizeof(SluaHashNode));
        if (!t->hash_part) SLUA_PANIC("out of memory: hash part alloc");
        t->hash_cap = TABLE_INIT_HASH_CAP;
    }
    uint32_t idx = hash_key(key) % (uint32_t)t->hash_cap;
    SluaHashNode* node = &t->hash_part[idx];
    if (node->key.tag == SLUA_TAG_NULL) {
        node->key = key;
        node->val = val;
        t->hash_count++;
        return;
    }
    SluaHashNode* cur = node;
    do {
        if (slua_key_equal(cur->key, key)) { cur->val = val; return; }
        if (!cur->next) break;
        cur = cur->next;
    } while (1);
    SluaHashNode* newnode = (SluaHashNode*)calloc(1, sizeof(SluaHashNode));
    if (!newnode) SLUA_PANIC("out of memory: hash node");
    newnode->key = key;
    newnode->val = val;
    cur->next = newnode;
    t->hash_count++;
}

int32_t slua_table_length(SluaTable* t) {
    return t ? t->array_size : 0;
}

void slua_table_insert(SluaTable* t, SluaValue val) {
    if (!t) return;
    SluaValue key = slua_int((int64_t)t->array_size + 1);
    slua_table_set(t, key, val);
}

SluaValue slua_table_remove(SluaTable* t, int32_t idx) {
    if (!t || idx < 1 || idx > t->array_size) return slua_null();
    SluaValue old = t->array_part[idx - 1];
    memmove(&t->array_part[idx - 1], &t->array_part[idx],
            sizeof(SluaValue) * (size_t)(t->array_size - idx));
    t->array_size--;
    return old;
}