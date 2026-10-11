/* Adapted from V7 usr/src/libc/stdio/gcvt.c; retain its Fortran F/E choice. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
char *gcvt(double value, int digits, char *buffer)
{
    /* Match the native ecvt precision ceiling and avoid the original indexing
     * outside its digit buffer for zero/negative/excessive counts. */
    if (digits < 1) digits = 1;
    if (digits > 64) digits = 64;
    int sign, point;
    char *input = ecvt(value, digits, &point, &sign);
    char *out = buffer;
    if (sign) *out++ = '-';
    if (!isfinite(value)) {
        strcpy(out, input);
        return buffer;
    }
    for (int i = digits - 1; i > 0 && input[i] == '0'; --i) --digits;
    if ((point >= 0 && point - digits > 4) || (point < 0 && point < -3)) {
        --point;
        *out++ = *input++;
        *out++ = '.';
        for (int i = 1; i < digits; ++i) *out++ = *input++;
        *out++ = 'e';
        if (point < 0) { point = -point; *out++ = '-'; }
        else *out++ = '+';
        /* V7 emitted two exponent digits. Binary64 also needs three. */
        if (point >= 100) *out++ = '0' + point / 100;
        *out++ = '0' + (point / 10) % 10;
        *out++ = '0' + point % 10;
    } else {
        if (point <= 0) {
            if (*input != '0') *out++ = '.';
            while (point < 0) { ++point; *out++ = '0'; }
        }
        for (int i = 1; i <= digits; ++i) {
            *out++ = *input++;
            if (i == point) *out++ = '.';
        }
        if (digits < point) {
            while (digits++ < point) *out++ = '0';
            *out++ = '.';
        }
    }
    if (out[-1] == '.') --out;
    *out = 0;
    return buffer;
}
