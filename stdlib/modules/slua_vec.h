#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
double slua_vec2_dot(double ax,double ay,double bx,double by);
double slua_vec2_len(double x,double y);
double slua_vec2_dist(double ax,double ay,double bx,double by);
void   slua_vec2_norm(double x,double y,double* ox,double* oy);
double slua_vec3_dot(double ax,double ay,double az,double bx,double by,double bz);
double slua_vec3_len(double x,double y,double z);
void   slua_vec3_cross(double ax,double ay,double az,double bx,double by,double bz,double* ox,double* oy,double* oz);
void   slua_vec3_norm(double x,double y,double z,double* ox,double* oy,double* oz);
double slua_vec3_dist(double ax,double ay,double az,double bx,double by,double bz);
double slua_vec3_cross_x(double ax,double ay,double az,double bx,double by,double bz);
double slua_vec3_cross_y(double ax,double ay,double az,double bx,double by,double bz);
double slua_vec3_cross_z(double ax,double ay,double az,double bx,double by,double bz);
double slua_vec3_norm_x(double x,double y,double z);
double slua_vec3_norm_y(double x,double y,double z);
double slua_vec3_norm_z(double x,double y,double z);
double slua_vec2_norm_x(double x,double y);
double slua_vec2_norm_y(double x,double y);
double slua_math_clamp(double v,double lo,double hi);
double slua_math_lerp(double a,double b,double t);
double slua_math_abs(double x);
double slua_math_floor(double x);
double slua_math_ceil(double x);
double slua_math_round(double x);
double slua_math_min2(double a,double b);
double slua_math_max2(double a,double b);
double slua_math_sign(double x);
double slua_math_fract(double x);
double slua_math_mod(double a,double b);
#ifdef __cplusplus
}
#endif
