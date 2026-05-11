#ifndef SARN_STDLIB_H
#define SARN_STDLIB_H

#include <stdint.h>
#include <stddef.h>
#include "../runtime/include/sarn_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

double sarn_sqrt(double x);
double sarn_pow(double base, double exp);
double sarn_sin(double x);
double sarn_cos(double x);
double sarn_tan(double x);
double sarn_log(double x);
double sarn_log2(double x);
double sarn_exp(double x);
double sarn_inf(void);
double sarn_nan(void);
double sarn_pi(void);
double sarn_e(void);

int32_t sarn_str_len(const char* s);
int32_t sarn_str_byte(const char* s, int32_t i);
char* sarn_str_char(int32_t b);
char* sarn_str_sub(const char* s, int32_t from, int32_t to);
char* sarn_int_to_str(int64_t n);
char* sarn_float_to_str(double x);
int64_t sarn_str_to_int(const char* s);
double sarn_str_to_float(const char* s);
char* sarn_str_upper(const char* s);
char* sarn_str_lower(const char* s);
int32_t sarn_str_find(const char* haystack, const char* needle, int32_t from);
char* sarn_str_trim(const char* s);

char* sarn_str_concat(const char* a, const char* b);
char* sarn_str_split(const char* s, const char* sep, int32_t index);
int32_t sarn_str_count(const char* s, const char* sep);

SarnTable* sarn_tbl_new(void);

void sarn_tbl_iset_i64(SarnTable* t, int64_t key, int64_t val);
void sarn_tbl_iset_f64(SarnTable* t, int64_t key, double val);
void sarn_tbl_iset_str(SarnTable* t, int64_t key, const char* val);
void sarn_tbl_iset_bool(SarnTable* t, int64_t key, int32_t val);

void sarn_tbl_sset_i64(SarnTable* t, const char* key, int64_t val);
void sarn_tbl_sset_f64(SarnTable* t, const char* key, double val);
void sarn_tbl_sset_str(SarnTable* t, const char* key, const char* val);
void sarn_tbl_sset_bool(SarnTable* t, const char* key, int32_t val);

int64_t sarn_tbl_iget_i64(SarnTable* t, int64_t key);
double sarn_tbl_iget_f64(SarnTable* t, int64_t key);
const char* sarn_tbl_iget_str(SarnTable* t, int64_t key);
int32_t sarn_tbl_iget_bool(SarnTable* t, int64_t key);

int64_t sarn_tbl_sget_i64(SarnTable* t, const char* key);
double sarn_tbl_sget_f64(SarnTable* t, const char* key);
const char* sarn_tbl_sget_str(SarnTable* t, const char* key);
int32_t sarn_tbl_sget_bool(SarnTable* t, const char* key);

void sarn_print_str(const char* s);
void sarn_print_int(int64_t n);
void sarn_print_float(double x);
void sarn_print_bool(int32_t b);
void sarn_print_null(void);
void sarn_print_str_no_newline(const char* s);
void sarn_write_bytes(const uint8_t* buf, int32_t len);
void sarn_flush(void);
char* sarn_read_line(void);
int32_t sarn_read_char(void);
void sarn_io_clear(void);
void sarn_io_set_color(const char* color);
void sarn_io_reset_color(void);
void sarn_io_print_color(const char* msg, const char* color);

#ifdef __cplusplus
}
#endif

#endif

int32_t sarn_os_is_admin(void);
int32_t sarn_os_add_to_path(const char* dir);
char*   sarn_os_get_temp_dir(void);
int64_t sarn_os_time();
void sarn_os_sleep(int64_t ms);
char* sarn_os_getenv(const char* key);
int64_t sarn_os_exit_code();
void sarn_os_system(const char* cmd);
char* sarn_os_cwd();
void sarn_os_sleepS(int64_t s);
#ifdef SARN_HAS_RAYLIB
void    sarn_window_init(int32_t w, int32_t h, const char* title);
void    sarn_window_close(void);
int32_t sarn_window_should_close(void);
void    sarn_begin_drawing(void);
void    sarn_end_drawing(void);
void    sarn_clear_bg(int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_set_target_fps(int32_t fps);
int32_t sarn_get_fps(void);
double  sarn_get_frame_time(void);
int32_t sarn_screen_width(void);
int32_t sarn_screen_height(void);
void    sarn_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_draw_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, int32_t thick, int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_draw_circle(int32_t cx, int32_t cy, float radius, int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_draw_circle_outline(int32_t cx, int32_t cy, float radius, int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t thick, int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_draw_triangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t r, int32_t g, int32_t b, int32_t a);
void    sarn_draw_text(const char* text, int32_t x, int32_t y, int32_t size, int32_t r, int32_t g, int32_t b, int32_t a);
int32_t sarn_measure_text(const char* text, int32_t size);
int32_t sarn_is_key_down(int32_t key);
int32_t sarn_is_key_pressed(int32_t key);
int32_t sarn_is_key_released(int32_t key);
int32_t sarn_get_mouse_x(void);
int32_t sarn_get_mouse_y(void);
int32_t sarn_is_mouse_btn_pressed(int32_t btn);
int32_t sarn_is_mouse_btn_down(int32_t btn);
double  sarn_get_mouse_wheel(void);
int32_t sarn_ui_button(int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
void    sarn_ui_label(int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
int32_t sarn_ui_checkbox(int32_t x, int32_t y, int32_t size, const char* text, int32_t checked);
double  sarn_ui_slider(int32_t x, int32_t y, int32_t w, int32_t h, double minv, double maxv, double val);
void    sarn_ui_progress_bar(int32_t x, int32_t y, int32_t w, int32_t h, double val, double maxv);
void    sarn_ui_panel(int32_t x, int32_t y, int32_t w, int32_t h, const char* title);
int32_t sarn_ui_text_input(int32_t x, int32_t y, int32_t w, int32_t h, char* buf, int32_t buf_size, int32_t active);
void    sarn_ui_set_font_size(int32_t size);
void    sarn_ui_set_accent(int32_t r, int32_t g, int32_t b);
#endif

#include "modules/sarn_fs.h"
#include "modules/sarn_random.h"
#include "modules/sarn_datetime.h"
#include "modules/sarn_path.h"
#include "modules/sarn_process.h"
#include "modules/sarn_json.h"
#include "modules/sarn_net.h"
#include "modules/sarn_sync.h"
#include "modules/sarn_regex.h"
#include "modules/sarn_crypto.h"
#include "modules/sarn_buf.h"
#include "modules/sarn_thread.h"
#include "modules/sarn_vec.h"
#include "modules/sarn_http.h"
#include "modules/sarn_tbl_extra.h"

