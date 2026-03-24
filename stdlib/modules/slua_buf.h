#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int64_t slua_buf_new(int32_t size);
int64_t slua_buf_from_str(const char* s, int32_t len);
int32_t slua_buf_free(int64_t id);
int32_t slua_buf_size(int64_t id);
int32_t slua_buf_write_u8(int64_t id, int32_t off, int32_t val);
int32_t slua_buf_write_u16(int64_t id, int32_t off, int32_t val);
int32_t slua_buf_write_u32(int64_t id, int32_t off, int32_t val);
int32_t slua_buf_write_i64(int64_t id, int32_t off, int64_t val);
int32_t slua_buf_write_f32(int64_t id, int32_t off, double val);
int32_t slua_buf_write_f64(int64_t id, int32_t off, double val);
int32_t slua_buf_read_u8(int64_t id, int32_t off);
int32_t slua_buf_read_u16(int64_t id, int32_t off);
int32_t slua_buf_read_u32_i(int64_t id, int32_t off);
int64_t slua_buf_read_i64(int64_t id, int32_t off);
double  slua_buf_read_f32(int64_t id, int32_t off);
double  slua_buf_read_f64(int64_t id, int32_t off);
char*   slua_buf_to_str(int64_t id);
char*   slua_buf_to_hex(int64_t id);
int32_t slua_buf_copy(int64_t dst, int32_t doff, int64_t src, int32_t soff, int32_t len);
int32_t slua_buf_fill(int64_t id, int32_t off, int32_t len, int32_t val);
int32_t slua_buf_write_str(int64_t id, int32_t off, const char* s);
#ifdef __cplusplus
}
#endif
