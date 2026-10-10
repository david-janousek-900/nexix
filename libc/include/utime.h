#ifndef NEXIX_LIBC_UTIME_H
#define NEXIX_LIBC_UTIME_H
#include <sys/types.h>
struct utimbuf { time_t actime, modtime; };
int utime(const char *, const struct utimbuf *);
#endif
