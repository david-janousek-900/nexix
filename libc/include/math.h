#ifndef NEXIX_LIBC_MATH_H
#define NEXIX_LIBC_MATH_H
#define HUGE 1.701411733192644270e38
#define LOGHUGE 39
#define INFINITY (__builtin_inf())
#define NAN (__builtin_nan(""))
#define HUGE_VAL INFINITY
#define isnan(x) __builtin_isnan(x)
#define isfinite(x) __builtin_isfinite(x)
#define signbit(x) __builtin_signbit(x)
double frexp(double, int *), ldexp(double, int), modf(double, double *);
double fabs(double), floor(double), ceil(double), fmod(double, double);
double sqrt(double), hypot(double, double), atof(const char *);
double sin(double), cos(double), tan(double), asin(double), acos(double);
double atan(double), atan2(double, double), exp(double), log(double), log10(double);
double pow(double, double), sinh(double), cosh(double), tanh(double);
double j0(double), j1(double), jn(int, double), y0(double), y1(double), yn(int, double);
double gamma(double);
extern int signgam;
struct v7_complex { double r, i; };
double v7_cabs(struct v7_complex);
#endif
