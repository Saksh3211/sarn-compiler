#include "sarn_vec.h"
#include <math.h>
double sarn_vec2_dot(double ax,double ay,double bx,double by) { return ax*bx+ay*by; }
double sarn_vec2_len(double x,double y) { return sqrt(x*x+y*y); }
double sarn_vec2_dist(double ax,double ay,double bx,double by) { return sarn_vec2_len(bx-ax,by-ay); }
static double _v2l(double x,double y){ double l=sqrt(x*x+y*y); return l>0?l:1e-10; }
double sarn_vec2_norm_x(double x,double y) { return x/_v2l(x,y); }
double sarn_vec2_norm_y(double x,double y) { return y/_v2l(x,y); }
void   sarn_vec2_norm(double x,double y,double* ox,double* oy) { double l=_v2l(x,y); *ox=x/l; *oy=y/l; }
double sarn_vec3_dot(double ax,double ay,double az,double bx,double by,double bz) { return ax*bx+ay*by+az*bz; }
double sarn_vec3_len(double x,double y,double z) { return sqrt(x*x+y*y+z*z); }
double sarn_vec3_dist(double ax,double ay,double az,double bx,double by,double bz) { return sarn_vec3_len(bx-ax,by-ay,bz-az); }
static double _v3l(double x,double y,double z){ double l=sqrt(x*x+y*y+z*z); return l>0?l:1e-10; }
double sarn_vec3_norm_x(double x,double y,double z) { return x/_v3l(x,y,z); }
double sarn_vec3_norm_y(double x,double y,double z) { return y/_v3l(x,y,z); }
double sarn_vec3_norm_z(double x,double y,double z) { return z/_v3l(x,y,z); }
void   sarn_vec3_norm(double x,double y,double z,double* ox,double* oy,double* oz) { double l=_v3l(x,y,z); *ox=x/l; *oy=y/l; *oz=z/l; }
double sarn_vec3_cross_x(double ax,double ay,double az,double bx,double by,double bz) { return ay*bz-az*by; }
double sarn_vec3_cross_y(double ax,double ay,double az,double bx,double by,double bz) { return az*bx-ax*bz; }
double sarn_vec3_cross_z(double ax,double ay,double az,double bx,double by,double bz) { return ax*by-ay*bx; }
void   sarn_vec3_cross(double ax,double ay,double az,double bx,double by,double bz,double* ox,double* oy,double* oz) { *ox=ay*bz-az*by; *oy=az*bx-ax*bz; *oz=ax*by-ay*bx; }
double sarn_math_clamp(double v,double lo,double hi) { return v<lo?lo:v>hi?hi:v; }
double sarn_math_lerp(double a,double b,double t) { return a+(b-a)*t; }
double sarn_math_abs(double x) { return x<0?-x:x; }
double sarn_math_floor(double x) { return floor(x); }
double sarn_math_ceil(double x) { return ceil(x); }
double sarn_math_round(double x) { return round(x); }
double sarn_math_min2(double a,double b) { return a<b?a:b; }
double sarn_math_max2(double a,double b) { return a>b?a:b; }
double sarn_math_sign(double x) { return x>0?1.0:x<0?-1.0:0.0; }
double sarn_math_fract(double x) { return x-floor(x); }
double sarn_math_mod(double a,double b) { return fmod(a,b); }
