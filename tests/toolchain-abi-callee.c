#include "toolchain-abi.h"
#include <stdarg.h>
struct abi_pair abi_mixed(struct abi_pair p)
{ p.integer += 7; p.real += .5; return p; }
struct abi_large abi_memory(long x)
{ struct abi_large p = {x, x + 1, x + 2}; return p; }
long double abi_extended(long double x) { return x + .25L; }
double abi_variadic(int marker, ...)
{
    va_list ap, copy;
    double total = marker;
    va_start(ap, marker); va_copy(copy, ap);
    for (int i = 0; i < 9; ++i) {
        long n = va_arg(ap, long);
        double d = va_arg(ap, double);
        if (n != va_arg(copy, long) || d != va_arg(copy, double)) total = -10000;
        total += n + d;
    }
    va_end(copy); va_end(ap); return total;
}
