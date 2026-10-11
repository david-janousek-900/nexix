#ifndef NEXIX_LIBC_SYS_TIMEB_H
#define NEXIX_LIBC_SYS_TIMEB_H
#include <sys/types.h>
struct timeb {
    time_t time;
    unsigned short millitm;
    short timezone, dstflag;
};
int ftime(struct timeb *);
#endif
