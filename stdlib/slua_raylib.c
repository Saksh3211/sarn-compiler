#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX

#ifdef SLUA_HAS_RAYLIB
#include <raylib.h>
#include <stdint.h>
#include <string.h>

static Color slua_mk_color(int32_t r, int32_t g, int32_t b, int32_t a) {
    Color c; c.r=(uint8_t)r; c.g=(uint8_t)g; c.b=(uint8_t)b; c.a=(uint8_t)a; return c;
}

static struct {
    int    font_size;
    Color  accent;
    Color  accent_hot;
    Color  text;
    Color  bg;
    Color  border;
} slua_ui = {
    16,
    {60,  120, 200, 255},
    {90,  150, 230, 255},
    {240, 240, 240, 255},
    {35,  35,  45,  240},
    {80,  80,  100, 255}
};

void slua_window_init(int32_t w, int32_t h, const char* title) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(w, h, title);
}
void    slua_window_close(void)             { CloseWindow(); }
int32_t slua_window_should_close(void)      { return WindowShouldClose() ? 1 : 0; }
void    slua_begin_drawing(void)            { BeginDrawing(); }
void    slua_end_drawing(void)              { EndDrawing(); }
void    slua_clear_bg(int32_t r, int32_t g, int32_t b, int32_t a) {
    ClearBackground(slua_mk_color(r, g, b, a));
}
void    slua_set_target_fps(int32_t fps)    { SetTargetFPS(fps); }
int32_t slua_get_fps(void)                  { return GetFPS(); }
double  slua_get_frame_time(void)           { return (double)GetFrameTime(); }
int32_t slua_screen_width(void)             { return GetScreenWidth(); }
int32_t slua_screen_height(void)            { return GetScreenHeight(); }

void slua_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h,
                    int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawRectangle(x, y, w, h, slua_mk_color(r, g, b, a));
}
void slua_draw_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h,
                             int32_t thick,
                             int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawRectangleLinesEx(
        (Rectangle){(float)x,(float)y,(float)w,(float)h},
        (float)thick, slua_mk_color(r, g, b, a));
}
void slua_draw_circle(int32_t cx, int32_t cy, float radius,
                      int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawCircle(cx, cy, radius, slua_mk_color(r, g, b, a));
}
void slua_draw_circle_outline(int32_t cx, int32_t cy, float radius,
                               int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawCircleLines(cx, cy, radius, slua_mk_color(r, g, b, a));
}
void slua_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                    int32_t thick,
                    int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawLineEx((Vector2){(float)x1,(float)y1},
               (Vector2){(float)x2,(float)y2},
               (float)thick, slua_mk_color(r, g, b, a));
}
void slua_draw_triangle(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                         int32_t x3, int32_t y3,
                         int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawTriangle((Vector2){(float)x1,(float)y1},
                 (Vector2){(float)x2,(float)y2},
                 (Vector2){(float)x3,(float)y3},
                 slua_mk_color(r, g, b, a));
}
void slua_draw_text(const char* text, int32_t x, int32_t y, int32_t size,
                    int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawText(text ? text : "", x, y, size, slua_mk_color(r, g, b, a));
}
int32_t slua_measure_text(const char* text, int32_t size) {
    return MeasureText(text ? text : "", size);
}

int32_t slua_is_key_down(int32_t k)            { return IsKeyDown(k)     ? 1 : 0; }
int32_t slua_is_key_pressed(int32_t k)         { return IsKeyPressed(k)  ? 1 : 0; }
int32_t slua_is_key_released(int32_t k)        { return IsKeyReleased(k) ? 1 : 0; }
int32_t slua_get_mouse_x(void)                 { return GetMouseX(); }
int32_t slua_get_mouse_y(void)                 { return GetMouseY(); }
int32_t slua_is_mouse_btn_pressed(int32_t btn) { return IsMouseButtonPressed(btn) ? 1 : 0; }
int32_t slua_is_mouse_btn_down(int32_t btn)    { return IsMouseButtonDown(btn)    ? 1 : 0; }
double  slua_get_mouse_wheel(void)             { return (double)GetMouseWheelMove(); }

void slua_ui_set_font_size(int32_t size) { slua_ui.font_size = size; }
void slua_ui_set_accent(int32_t r, int32_t g, int32_t b) {
    slua_ui.accent = slua_mk_color(r, g, b, 255);
    int rh = r + 40; if (rh > 255) rh = 255;
    int gh = g + 40; if (gh > 255) gh = 255;
    int bh = b + 40; if (bh > 255) bh = 255;
    slua_ui.accent_hot = slua_mk_color(rh, gh, bh, 255);
}

int32_t slua_ui_button(int32_t x, int32_t y, int32_t w, int32_t h,const char* text) {
    int mx = GetMouseX(), my = GetMouseY();
    int hover = mx >= x && mx <= x+w && my >= y && my <= y+h;
    DrawRectangle(x, y, w, h, hover ? slua_ui.accent_hot : slua_ui.accent);
    DrawRectangleLinesEx(
        (Rectangle){(float)x,(float)y,(float)w,(float)h},
        1.5f, slua_ui.border);
    int tw = MeasureText(text, slua_ui.font_size);
    DrawText(text, x + (w-tw)/2, y + (h-slua_ui.font_size)/2,slua_ui.font_size, slua_ui.text);
    return (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ? 1 : 0;
}

void slua_ui_label(int32_t x, int32_t y, int32_t w, int32_t h,const char* text) {
    (void)w;
    DrawText(text, x, y + (h-slua_ui.font_size)/2,slua_ui.font_size, slua_ui.text);
}

int32_t slua_ui_checkbox(int32_t x, int32_t y, int32_t size,const char* text, int32_t checked) {
    int mx = GetMouseX(), my = GetMouseY();
    int hover = mx >= x && mx <= x+size && my >= y && my <= y+size;
    DrawRectangle(x, y, size, size, slua_ui.bg);
    DrawRectangleLinesEx(
        (Rectangle){(float)x,(float)y,(float)size,(float)size},
        1.5f, hover ? slua_ui.accent_hot : slua_ui.border);
    if (checked) {
        int pad = size / 4;
        DrawRectangle(x+pad, y+pad, size-pad*2, size-pad*2, slua_ui.accent);
    }
    DrawText(text, x+size+6, y+(size-slua_ui.font_size)/2,slua_ui.font_size, slua_ui.text);
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return checked ? 0 : 1;
    return checked;
}

double slua_ui_slider(int32_t x, int32_t y, int32_t w, int32_t h,double minv, double maxv, double val) {
    int mx = GetMouseX(), my = GetMouseY();
    DrawRectangle(x, y+h/2-2, w, 4, slua_ui.border);
    double t = (val - minv) / (maxv - minv);
    int kx = x + (int)(t * w);
    int kr = h / 2;
    int hover = mx >= kx-kr && mx <= kx+kr && my >= y && my <= y+h;
    DrawCircle(kx, y+h/2, (float)kr,hover ? slua_ui.accent_hot : slua_ui.accent);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        mx >= x && mx <= x+w && my >= y && my <= y+h) {
        t = (double)(mx - x) / w;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        return minv + t * (maxv - minv);
    }
    return val;
}

void slua_ui_progress_bar(int32_t x, int32_t y, int32_t w, int32_t h,
                           double val, double maxv) {
    DrawRectangle(x, y, w, h, slua_ui.bg);
    int filled = (maxv > 0.0) ? (int)((val / maxv) * w) : 0;
    if (filled > 0) DrawRectangle(x, y, filled, h, slua_ui.accent);
    DrawRectangleLinesEx(
        (Rectangle){(float)x,(float)y,(float)w,(float)h},
        1.0f, slua_ui.border);
}

void slua_ui_panel(int32_t x, int32_t y, int32_t w, int32_t h,
                   const char* title) {
    DrawRectangle(x, y, w, h, (Color){30,30,40,220});
    if (title && title[0]) {
        DrawRectangle(x, y, w, slua_ui.font_size+10, slua_ui.accent);
        DrawText(title, x+8, y+5, slua_ui.font_size, slua_ui.text);
    }
    DrawRectangleLinesEx(
        (Rectangle){(float)x,(float)y,(float)w,(float)h},
        1.5f, slua_ui.border);
}

int32_t slua_ui_text_input(int32_t x, int32_t y, int32_t w, int32_t h,
                            char* buf, int32_t buf_size, int32_t active) {
    int mx = GetMouseX(), my = GetMouseY();
    int hover = mx >= x && mx <= x+w && my >= y && my <= y+h;
    Color bc = active ? slua_ui.accent
             : (hover ? slua_ui.accent_hot : slua_ui.border);
    DrawRectangle(x, y, w, h, slua_ui.bg);
    DrawRectangleLinesEx(
        (Rectangle){(float)x,(float)y,(float)w,(float)h},
        active ? 2.0f : 1.5f, bc);
    if (buf) {
        DrawText(buf, x+6, y+(h-slua_ui.font_size)/2,
                 slua_ui.font_size, slua_ui.text);
        if (active) {
            int key = GetCharPressed();
            int len = (int)strlen(buf);
            if (key >= 32 && key < 127 && len < buf_size-1) {
                buf[len]   = (char)key;
                buf[len+1] = '\0';
            }
            if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
                buf[len-1] = '\0';
        }
    }
    if (hover  && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 1;
    if (!hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;
    return active;
}


#define SLUA_MAX_FONTS 32
static Font slua_font_reg[SLUA_MAX_FONTS];
static int  slua_font_used[SLUA_MAX_FONTS];
static int  slua_font_inited = 0;

static void slua_font_reg_init(void) {
    if (!slua_font_inited) {
        int i; for (i = 0; i < SLUA_MAX_FONTS; i++) slua_font_used[i] = 0;
        slua_font_inited = 1;
    }
}

int32_t slua_font_load(const char* path, int32_t size) {
    int i;
    slua_font_reg_init();
    for (i = 0; i < SLUA_MAX_FONTS; i++) {
        if (!slua_font_used[i]) {
            slua_font_reg[i] = LoadFontEx(path, size, NULL, 0);
            if (slua_font_reg[i].texture.id == 0) return -1;
            slua_font_used[i] = 1;
            return (int32_t)i;
        }
    }
    return -1;
}

void slua_font_unload(int32_t id) {
    if (id < 0 || id >= SLUA_MAX_FONTS || !slua_font_used[id]) return;
    UnloadFont(slua_font_reg[id]);
    slua_font_used[id] = 0;
}

void slua_draw_text_font(int32_t font_id, const char* text,int32_t x, int32_t y, int32_t size, float spacing,
        int32_t r, int32_t g, int32_t b, int32_t a) {
    Color col = slua_mk_color(r, g, b, a);
    const char* t = text ? text : "";
    if (font_id < 0 || font_id >= SLUA_MAX_FONTS || !slua_font_used[font_id]) {
        DrawText(t, x, y, size, col);
        return;
    }
    Font* f = &slua_font_reg[font_id];
    float fs = (size > 0) ? (float)size : (float)f->baseSize;
    DrawTextEx(*f, t, (Vector2){(float)x, (float)y}, fs, spacing, col);
}
#endif
