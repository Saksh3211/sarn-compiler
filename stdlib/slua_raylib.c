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

#ifdef SLUA_HAS_RAYLIB
#include <raylib.h>
#include <raymath.h>
#include <stdlib.h>
#include <string.h>

static Camera3D _cam3d = {0};

void slua_camera3d_set(double px, double py, double pz,double tx, double ty, double tz,
        double ux, double uy, double uz,
        double fovy, int32_t proj) {
    _cam3d.position   = (Vector3){(float)px,(float)py,(float)pz};
    _cam3d.target     = (Vector3){(float)tx,(float)ty,(float)tz};
    _cam3d.up         = (Vector3){(float)ux,(float)uy,(float)uz};
    _cam3d.fovy       = (float)fovy;
    _cam3d.projection = proj;
}
void slua_camera3d_update(void)  { UpdateCamera(&_cam3d, CAMERA_FREE); }
void slua_begin_mode3d(void)  { BeginMode3D(_cam3d); }
void slua_end_mode3d(void)  { EndMode3D(); }
void slua_draw_cube(float px,float py,float pz,float w,float h,float d,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawCube((Vector3){px,py,pz},w,h,d,slua_mk_color(r,g,b,a));
}
void slua_draw_cube_wires(float px,float py,float pz,float w,float h,float d,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawCubeWires((Vector3){px,py,pz},w,h,d,slua_mk_color(r,g,b,a));
}
void slua_draw_sphere(float px,float py,float pz,float radius,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawSphere((Vector3){px,py,pz},radius,slua_mk_color(r,g,b,a));
}
void slua_draw_sphere_wires(float px,float py,float pz,float radius,int32_t rings,int32_t slices,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawSphereWires((Vector3){px,py,pz},radius,rings,slices,slua_mk_color(r,g,b,a));
}
void slua_draw_plane(float px,float py,float pz,float w,float h,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawPlane((Vector3){px,py,pz},(Vector2){w,h},slua_mk_color(r,g,b,a));
}
void slua_draw_cylinder(float px,float py,float pz,float rt,float rb,float height,int32_t slices,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawCylinder((Vector3){px,py,pz},rt,rb,height,slices,slua_mk_color(r,g,b,a));
}
void slua_draw_grid(int32_t slices, float spacing) { DrawGrid(slices,spacing); }
void slua_draw_ray(float px,float py,float pz,float dx,float dy,float dz,int32_t r,int32_t g,int32_t b,int32_t a) {
    Ray ray; ray.position=(Vector3){px,py,pz}; ray.direction=(Vector3){dx,dy,dz};
    DrawRay(ray,slua_mk_color(r,g,b,a));
}
void slua_draw_line3d(float x1,float y1,float z1,float x2,float y2,float z2,int32_t r,int32_t g,int32_t b,int32_t a) {
    DrawLine3D((Vector3){x1,y1,z1},(Vector3){x2,y2,z2},slua_mk_color(r,g,b,a));
}

#define MDL_MAX 64
static Model _models[MDL_MAX];
static int   _mused[MDL_MAX];
static int   _minit;
static void _m_init(void) { if(!_minit){ memset(_mused,0,sizeof(_mused)); _minit=1; } }
int32_t slua_model_load(const char* path) {
    _m_init(); for(int i=0;i<MDL_MAX;i++) if(!_mused[i]){ _models[i]=LoadModel(path); _mused[i]=1; return i; } return -1;
}
void slua_model_draw(int32_t id,float px,float py,float pz,float scale,int32_t r,int32_t g,int32_t b,int32_t a) {
    if(id<0||id>=MDL_MAX||!_mused[id]) return;
    DrawModel(_models[id],(Vector3){px,py,pz},scale,slua_mk_color(r,g,b,a));
}
void slua_model_draw_ex(int32_t id,float px,float py,float pz,float ax,float ay,float az,float angle,float sx,float sy,float sz,int32_t r,int32_t g,int32_t b,int32_t a) {
    if(id<0||id>=MDL_MAX||!_mused[id]) return;
    DrawModelEx(_models[id],(Vector3){px,py,pz},(Vector3){ax,ay,az},angle,(Vector3){sx,sy,sz},slua_mk_color(r,g,b,a));
}
void slua_model_unload(int32_t id) {
    if(id<0||id>=MDL_MAX||!_mused[id]) return;
    UnloadModel(_models[id]); _mused[id]=0;
}

#define TEX_MAX 64
static Texture2D _texs[TEX_MAX]; static int _tused[TEX_MAX]; static int _ttinit;
static void _tt_init(void){ if(!_ttinit){ memset(_tused,0,sizeof(_tused)); _ttinit=1; } }
int32_t slua_texture_load(const char* path) {
    _tt_init(); for(int i=0;i<TEX_MAX;i++) if(!_tused[i]){ _texs[i]=LoadTexture(path); _tused[i]=1; return i; } return -1;
}
void slua_texture_draw(int32_t id,int32_t x,int32_t y,int32_t r,int32_t g,int32_t b,int32_t a) {
    if(id<0||id>=TEX_MAX||!_tused[id]) return;
    DrawTexture(_texs[id],x,y,slua_mk_color(r,g,b,a));
}
void slua_texture_draw_ex(int32_t id,float x,float y,float angle,float scale,int32_t r,int32_t g,int32_t b,int32_t a) {
    if(id<0||id>=TEX_MAX||!_tused[id]) return;
    DrawTextureEx(_texs[id],(Vector2){x,y},angle,scale,slua_mk_color(r,g,b,a));
}
void slua_texture_unload(int32_t id) {
    if(id<0||id>=TEX_MAX||!_tused[id]) return; UnloadTexture(_texs[id]); _tused[id]=0;
}
int32_t slua_texture_width(int32_t id)  { return (id>=0&&id<TEX_MAX&&_tused[id])?_texs[id].width:0; }
int32_t slua_texture_height(int32_t id) { return (id>=0&&id<TEX_MAX&&_tused[id])?_texs[id].height:0; }

void slua_window_init_3d(int32_t w, int32_t h, const char* title) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(w, h, title);
}
double slua_get_time(void) { return GetTime(); }
void   slua_draw_fps_counter(int32_t x, int32_t y) { DrawFPS(x, y); }
float  slua_vec3_dot_f(float ax,float ay,float az,float bx,float by,float bz) { return ax*bx+ay*by+az*bz; }

#endif
