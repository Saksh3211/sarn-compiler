#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void    sarn_random_seed(int64_t seed);
int64_t sarn_random_int(int64_t lo, int64_t hi);
double  sarn_random_float(void);
int64_t sarn_random_range(int64_t lo, int64_t hi);
double  sarn_random_gauss(double mean, double stddev);
#ifdef __cplusplus
}
#endif
