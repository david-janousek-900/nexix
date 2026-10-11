/* IEEE binary64 replacements for the V7 PDP-11 FP assembly helpers. */
#include <math.h>
#include <stdint.h>
#include <errno.h>
union binary64 { double value; uint64_t bits; };
double modf(double value, double *integer)
{
    union binary64 x = {.value = value}, zero = {.bits = x.bits & (1ULL << 63)};
    int e = (int)((x.bits >> 52) & 2047) - 1023;
    if (e < 0) { *integer = zero.value; return value; }
    if (e >= 52) { *integer = value; return isnan(value) ? value : zero.value; }
    x.bits &= ~((1ULL << (52 - e)) - 1);
    *integer = x.value;
    double fraction = value - x.value;
    return fraction == 0 ? zero.value : fraction;
}
double frexp(double value, int *exponent)
{
    union binary64 x = {.value = value};
    int e = (int)((x.bits >> 52) & 2047);
    *exponent = 0;
    if (value == 0 || e == 2047) return value;
    if (!e) { x.value *= 0x1p54; e = (int)((x.bits >> 52) & 2047); *exponent = -54; }
    *exponent += e - 1022;
    x.bits = (x.bits & ~(2047ULL << 52)) | (1022ULL << 52);
    return x.value;
}
double ldexp(double value, int exponent)
{
    if (!isfinite(value) || value == 0) return value;
    double original = value;
    if (exponent > 4096) { errno = ERANGE; return signbit(value) ? -INFINITY : INFINITY; }
    if (exponent < -4096) { errno = ERANGE; return signbit(value) ? -0.0 : 0.0; }
    while (exponent > 512) { value *= 0x1p512; exponent -= 512; }
    while (exponent < -512) { value *= 0x1p-512; exponent += 512; }
    union binary64 scale = {.bits = (uint64_t)(exponent + 1023) << 52};
    value *= scale.value;
    if (!isfinite(value) || (original != 0 && value == 0)) errno = ERANGE;
    return value;
}
double fmod(double value, double divisor)
{
    if (isnan(value) || isnan(divisor)) return value + divisor;
    if (!isfinite(value) || divisor == 0) { errno = EDOM; return NAN; }
    if (!isfinite(divisor)) return value;
    double result;
    /* FPREM may require several iterations for a large exponent difference. */
    __asm__ volatile ("1: fprem; fnstsw %%ax; test $0x400, %%ax; jne 1b; fstp %%st(1)"
        : "=t"(result) : "0"(value), "u"(divisor) : "ax", "cc", "st(1)");
    return result;
}
