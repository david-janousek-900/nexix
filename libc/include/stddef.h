#ifndef NEXIX_LIBC_STDDEF_H
#define NEXIX_LIBC_STDDEF_H
typedef unsigned long size_t;
typedef long ptrdiff_t;
typedef int wchar_t;
#define NULL ((void *)0)
#define offsetof(type,member) __builtin_offsetof(type,member)
typedef struct { long long __integer; long double __floating; } max_align_t;
#endif
