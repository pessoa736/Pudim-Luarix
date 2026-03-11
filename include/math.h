#ifndef KERNEL_MATH_H
#define KERNEL_MATH_H

int abs(int x);

double fabs(double x);
double floor(double x);
double ceil(double x);
double fmod(double x, double y);
double pow(double x, double y);
double ldexp(double x, int exp);
double frexp(double x, int* exp);

double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan2(double y, double x);
double log(double x);
double log2(double x);
double log10(double x);
double exp(double x);

#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())

#endif
