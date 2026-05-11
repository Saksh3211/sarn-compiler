#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
double sarn_vec2_dot(double ax,double ay,double bx,double by);
double sarn_vec2_len(double x,double y);
double sarn_vec2_dist(double ax,double ay,double bx,double by);
void   sarn_vec2_norm(double x,double y,double* ox,double* oy);
double sarn_vec3_dot(double ax,double ay,double az,double bx,double by,double bz);
double sarn_vec3_len(double x,double y,double z);
void   sarn_vec3_cross(double ax,double ay,double az,double bx,double by,double bz,double* ox,double* oy,double* oz);
void   sarn_vec3_norm(double x,double y,double z,double* ox,double* oy,double* oz);
double sarn_vec3_dist(double ax,double ay,double az,double bx,double by,double bz);
double sarn_vec3_cross_x(double ax,double ay,double az,double bx,double by,double bz);
double sarn_vec3_cross_y(double ax,double ay,double az,double bx,double by,double bz);
double sarn_vec3_cross_z(double ax,double ay,double az,double bx,double by,double bz);
double sarn_vec3_norm_x(double x,double y,double z);
double sarn_vec3_norm_y(double x,double y,double z);
double sarn_vec3_norm_z(double x,double y,double z);
double sarn_vec2_norm_x(double x,double y);
double sarn_vec2_norm_y(double x,double y);
double sarn_math_clamp(double v,double lo,double hi);
double sarn_math_lerp(double a,double b,double t);
double sarn_math_abs(double x);
double sarn_math_floor(double x);
double sarn_math_ceil(double x);
double sarn_math_round(double x);
double sarn_math_min2(double a,double b);
double sarn_math_max2(double a,double b);
double sarn_math_sign(double x);
double sarn_math_fract(double x);
double sarn_math_mod(double a,double b);
#ifdef __cplusplus
}
#endif
