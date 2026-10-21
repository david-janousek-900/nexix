#include "toolchain-abi.h"
#include <ident.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#define LAYOUT(t,n) _Static_assert(sizeof(t)==n && _Alignof(t)==n, #t)
LAYOUT(char,1); LAYOUT(short,2); LAYOUT(int,4); LAYOUT(long,8);
LAYOUT(long long,8); LAYOUT(void *,8); LAYOUT(float,4); LAYOUT(double,8);
LAYOUT(long double,16); LAYOUT(size_t,8); LAYOUT(ptrdiff_t,8);
_Static_assert(CHAR_BIT==8 && CHAR_MIN<0, "signed eight-bit default char");
_Static_assert(offsetof(struct abi_pair,real)==8 && sizeof(struct abi_pair)==16, "aggregate layout");
static long zero[64];
int main(int argc, char **argv, char **envp)
{
    struct abi_pair p = {10, 1.25};
    struct abi_large large;
    if (argc != 2 || strcmp(argv[1],"native") || !envp || !getenv("ABI_TEST") ||
        strcmp(getenv("ABI_TEST"),"yes") || strcmp(myname,"nexix x86_64") ||
        strcmp(NEXIX_TOOLCHAIN_TARGET,"x86_64-nexix") || NEXIX_TOOLCHAIN_ABI!=1) return 1;
    for (unsigned i=0; i<64; ++i) if (zero[i]) return 2;
    p = abi_mixed(p); large = abi_memory(0x123456789L);
    if (p.integer!=17 || p.real!=1.75 || large.a!=0x123456789L ||
        large.b!=0x12345678aL || large.c!=0x12345678bL || abi_extended(1.5L)!=1.75L) return 3;
    if (abi_inspect(1L,2L,3L,4L,5L,6L,7L,8L,1.,2.,3.,4.,5.,6.,7.,8.,9.)) return 4;
    if (abi_variadic(1,1L,1.,2L,2.,3L,3.,4L,4.,5L,5.,6L,6.,7L,7.,8L,8.,9L,9.)!=91.) return 5;
    puts("TOOLCHAIN_ABI_OK"); return 0;
}
