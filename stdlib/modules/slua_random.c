#include "slua_random.h"
#include <stdlib.h>
#include <math.h>
void    slua_random_seed(int64_t s) { srand((unsigned int)s); }
int64_t slua_random_int(int64_t lo, int64_t hi) { if (hi<=lo) return lo; return lo+(int64_t)(rand()%(int)(hi-lo+1)); }
double  slua_random_float(void) { return (double)rand()/(double)RAND_MAX; }
int64_t slua_random_range(int64_t lo, int64_t hi) { return slua_random_int(lo, hi); }
double  slua_random_gauss(double mean, double stddev) {
    double u = slua_random_float(), v = slua_random_float();
    if (u == 0.0) u = 1e-10;
    return mean + stddev * sqrt(-2.0*log(u)) * cos(6.28318530718*v);
}
