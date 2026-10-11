/* Decimal conversion for the native binary64 ABI. */
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
static int digit(int c) { return c >= '0' && c <= '9'; }
double strtod(const char *text, char **end)
{
    const char *p = text;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) ++p;
    int negative = 0;
    if (*p == '+' || *p == '-') negative = *p++ == '-';
    long double value = 0;
    int exponent = 0, fraction = 0, digits = 0, retained = 0;
    while (digit(*p) || (!fraction && *p == '.')) {
        if (*p == '.') { fraction = 1; ++p; continue; }
        ++digits;
        if (retained < 19) {
            value = value * 10 + (*p - '0');
            if (value != 0) ++retained;
            if (fraction && exponent > -10000) --exponent;
        } else if (!fraction && exponent < 10000) ++exponent;
        ++p;
    }
    if (!digits) { if (end) *end = (char *)text; return 0; }
    if (*p == 'e' || *p == 'E') {
        const char *mark = p++, *numbers;
        int minus = 0, power = 0;
        if (*p == '+' || *p == '-') minus = *p++ == '-';
        numbers = p;
        while (digit(*p)) { if (power < 10000) power = power * 10 + *p - '0'; ++p; }
        if (p == numbers) p = mark;
        else exponent += minus ? -power : power;
    }
    if (end) *end = (char *)p;
    if (value != 0) {
        if (exponent > 5000) value = INFINITY;
        else if (exponent < -5000) value = 0;
        else {
            while (exponent > 0) { value *= 10; --exponent; }
            while (exponent < 0) { value /= 10; ++exponent; }
        }
    }
    double result = (double)value;
    if (!isfinite(result) || (retained && (result == 0 || (result < 0x1p-1022)))) errno = ERANGE;
    return negative ? -result : result;
}
double atof(const char *text) { return strtod(text, NULL); }
/* V7's shared-buffer ecvt/fcvt contract, enlarged for IEEE binary64's exponent
 * range. Decimal rounding is the historical half-up rule, not printf's C99
 * correctly-rounded contract. */
static char converted[768];
static char *convert(double value, int count, int fixed, int *point, int *negative)
{
    *negative = !!signbit(value);
    if (*negative) value = -value;
    if (!isfinite(value)) { *point = 0; strcpy(converted, isnan(value) ? "nan" : "inf"); return converted; }
    long double scaled = value;
    int decimal = 1;
    if (scaled != 0) {
        while (scaled >= 10) { scaled /= 10; ++decimal; }
        while (scaled < 1) { scaled *= 10; --decimal; }
    }
    *point = value == 0 ? 0 : decimal;
    if (count < 0) count = 0;
    if (count > 64) count = 64;
    int n = fixed ? count + *point : count;
    if (n < 0) { converted[0] = 0; return converted; }
    if (n > 766) n = 766;
    for (int i = 0; i <= n; ++i) {
        int d = (int)scaled;
        if (d > 9) d = 9;
        converted[i] = '0' + d;
        scaled = (scaled - d) * 10;
    }
    int i = n;
    if (converted[i] >= '5') {
        while (i && converted[i - 1] == '9') converted[--i] = '0';
        if (i) ++converted[i - 1];
        else {
            memmove(converted + 1, converted, (size_t)n);
            converted[0] = '1'; ++*point;
            if (fixed) ++n;
        }
    }
    converted[n] = 0;
    return converted;
}
char *ecvt(double value, int count, int *point, int *negative)
{ return convert(value, count, 0, point, negative); }
char *fcvt(double value, int count, int *point, int *negative)
{ return convert(value, count, 1, point, negative); }
