#ifndef SLUA_STDLIB_H
#define SLUA_STDLIB_H

#include <stdint.h>
#include <stddef.h>
#include "../runtime/include/slua_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

double slua_sqrt(double x);
double slua_pow(double base, double exp);
double slua_sin(double x);
double slua_cos(double x);
double slua_tan(double x);
double slua_log(double x);
double slua_log2(double x);
double slua_exp(double x);
double slua_inf(void);
double slua_nan(void);
double slua_pi(void);
double slua_e(void);

int32_t slua_str_len(const char* s);
int32_t slua_str_byte(const char* s, int32_t i);
char* slua_str_char(int32_t b);
char* slua_str_sub(const char* s, int32_t from, int32_t to);
char* slua_int_to_str(int64_t n);
char* slua_float_to_str(double x);
int64_t slua_str_to_int(const char* s);
double slua_str_to_float(const char* s);
char* slua_str_upper(const char* s);
char* slua_str_lower(const char* s);
int32_t slua_str_find(const char* haystack, const char* needle, int32_t from);
char* slua_str_trim(const char* s);

char* slua_str_concat(const char* a, const char* b);
char* slua_str_split(const char* s, const char* sep, int32_t index);
int32_t slua_str_count(const char* s, const char* sep);

SluaTable* slua_tbl_new(void);

void slua_tbl_iset_i64(SluaTable* t, int64_t key, int64_t val);
void slua_tbl_iset_f64(SluaTable* t, int64_t key, double val);
void slua_tbl_iset_str(SluaTable* t, int64_t key, const char* val);
void slua_tbl_iset_bool(SluaTable* t, int64_t key, int32_t val);

void slua_tbl_sset_i64(SluaTable* t, const char* key, int64_t val);
void slua_tbl_sset_f64(SluaTable* t, const char* key, double val);
void slua_tbl_sset_str(SluaTable* t, const char* key, const char* val);
void slua_tbl_sset_bool(SluaTable* t, const char* key, int32_t val);

int64_t slua_tbl_iget_i64(SluaTable* t, int64_t key);
double slua_tbl_iget_f64(SluaTable* t, int64_t key);
const char* slua_tbl_iget_str(SluaTable* t, int64_t key);
int32_t slua_tbl_iget_bool(SluaTable* t, int64_t key);

int64_t slua_tbl_sget_i64(SluaTable* t, const char* key);
double slua_tbl_sget_f64(SluaTable* t, const char* key);
const char* slua_tbl_sget_str(SluaTable* t, const char* key);
int32_t slua_tbl_sget_bool(SluaTable* t, const char* key);

void slua_print_str(const char* s);
void slua_print_int(int64_t n);
void slua_print_float(double x);
void slua_print_bool(int32_t b);
void slua_print_null(void);
void slua_print_str_no_newline(const char* s);
void slua_write_bytes(const uint8_t* buf, int32_t len);
void slua_flush(void);
char* slua_read_line(void);
int32_t slua_read_char(void);
void slua_io_clear(void);
void slua_io_set_color(const char* color);
void slua_io_reset_color(void);
void slua_io_print_color(const char* msg, const char* color);

#ifdef __cplusplus
}
#endif

#endif

int64_t slua_os_time();
void slua_os_sleep(int64_t ms);
char* slua_os_getenv(const char* key);
int64_t slua_os_exit_code();
void slua_os_system(const char* cmd);
char* slua_os_cwd();
void slua_os_sleepS(int64_t s);
#ifdef SLUA_HAS_RAYLIB
void    slua_window_init(int32_t w, int32_t h, const char* title);
void    slua_window_close(void);
int32_t slua_window_should_close(void);
void    slua_begin_drawing(void);
void    slua_end_drawing(void);
void    slua_clear_bg(int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_set_target_fps(int32_t fps);
int32_t slua_get_fps(void);
double  slua_get_frame_time(void);
int32_t slua_screen_width(void);
int32_t slua_screen_height(void);
void    slua_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_draw_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, int32_t thick, int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_draw_circle(int32_t cx, int32_t cy, float radius, int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_draw_circle_outline(int32_t cx, int32_t cy, float radius, int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t thick, int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_draw_triangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t r, int32_t g, int32_t b, int32_t a);
void    slua_draw_text(const char* text, int32_t x, int32_t y, int32_t size, int32_t r, int32_t g, int32_t b, int32_t a);
int32_t slua_measure_text(const char* text, int32_t size);
int32_t slua_is_key_down(int32_t key);
int32_t slua_is_key_pressed(int32_t key);
int32_t slua_is_key_released(int32_t key);
int32_t slua_get_mouse_x(void);
int32_t slua_get_mouse_y(void);
int32_t slua_is_mouse_btn_pressed(int32_t btn);
int32_t slua_is_mouse_btn_down(int32_t btn);
double  slua_get_mouse_wheel(void);
int32_t slua_ui_button(int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
void    slua_ui_label(int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
int32_t slua_ui_checkbox(int32_t x, int32_t y, int32_t size, const char* text, int32_t checked);
double  slua_ui_slider(int32_t x, int32_t y, int32_t w, int32_t h, double minv, double maxv, double val);
void    slua_ui_progress_bar(int32_t x, int32_t y, int32_t w, int32_t h, double val, double maxv);
void    slua_ui_panel(int32_t x, int32_t y, int32_t w, int32_t h, const char* title);
int32_t slua_ui_text_input(int32_t x, int32_t y, int32_t w, int32_t h, char* buf, int32_t buf_size, int32_t active);
void    slua_ui_set_font_size(int32_t size);
void    slua_ui_set_accent(int32_t r, int32_t g, int32_t b);
#endif

#include "modules/slua_fs.h"
#include "modules/slua_random.h"
#include "modules/slua_datetime.h"
#include "modules/slua_path.h"
#include "modules/slua_process.h"
#include "modules/slua_json.h"
#include "modules/slua_net.h"
#include "modules/slua_sync.h"
#include "modules/slua_regex.h"
#include "modules/slua_crypto.h"
#include "modules/slua_buf.h"
#include "modules/slua_thread.h"
#include "modules/slua_vec.h"
#include "modules/slua_http.h"
#include "modules/slua_tbl_extra.h"
