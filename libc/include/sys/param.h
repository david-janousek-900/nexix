#ifndef NEXIX_LIBC_SYS_PARAM_H
#define NEXIX_LIBC_SYS_PARAM_H
#include <sys/types.h>
#include <sys/stat.h>
#define major(d) (((unsigned)(d)>>8)&255)
#define minor(d) ((unsigned)(d)&255)
#define HZ 100
#endif
