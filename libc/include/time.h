#ifndef NEXIX_LIBC_TIME_H
#define NEXIX_LIBC_TIME_H
#include <sys/types.h>
time_t time(time_t *);
int stime(const time_t *);
#endif
