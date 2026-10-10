#include <string.h>
#include <stdint.h>
void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if ((uintptr_t)d < (uintptr_t)s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else if (d != s) {
        while (n) { --n; d[n] = s[n]; }
    }
    return dst;
}
void *memset(void *dst, int value, size_t n)
{
    unsigned char *d = dst;
    for (size_t i = 0; i < n; ++i) d[i] = (unsigned char)value;
    return dst;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a, *q = b;
    for (size_t i = 0; i < n; ++i) if (p[i] != q[i]) return (int)p[i] - q[i];
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
char *strcpy(char *restrict dst, const char *restrict src)
{ size_t i = 0; do { dst[i] = src[i]; } while (src[i++]); return dst; }
char *strncpy(char *restrict dst, const char *restrict src, size_t n)
{
    size_t i = 0;
    while (i < n && src[i]) { dst[i] = src[i]; ++i; }
    while (i < n) dst[i++] = 0;
    return dst;
}
char *strchr(const char *s, int c)
{
    char value = (char)c;
    do { if (*s == value) return (char *)s; } while (*s++);
    return NULL;
}
char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    char value = (char)c;
    do { if (*s == value) last = s; } while (*s++);
    return (char *)last;
}
