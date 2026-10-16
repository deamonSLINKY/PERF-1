#pragma once
#include <stddef.h>
static inline float fabsf(float x) { return __builtin_fabsf(x); }
static inline double fabs(double x) { return __builtin_fabs(x); }
double log10(double x);
double sqrt(double x);
float sqrtf(float x);
