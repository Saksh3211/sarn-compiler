#include <stdint.h>

int64_t _cmath_add(int64_t a, int64_t b) { return a + b; }
int64_t _cmath_mul(int64_t a, int64_t b) { return a * b; }
int64_t _cmath_sub(int64_t a, int64_t b) { return a - b; }
double  _cmath_div(double a, double b)   { return b != 0.0 ? a / b : 0.0; }
int64_t _cmath_pow_i(int64_t b, int64_t e) {
     int64_t r = 1;
     for (int64_t i = 0; i < e; i++) r *= b;
     return r;
}
