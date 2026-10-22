#ifndef NEXIX_LIBC_SYS_TIME_H
#define NEXIX_LIBC_SYS_TIME_H
#include <sys/types.h>
#include <nexix/socket.h>
/* Only the interval record used by SO_SNDTIMEO/SO_RCVTIMEO (task 424). */
struct timeval { time_t tv_sec; long tv_usec; };
_Static_assert(sizeof(struct timeval) == sizeof(struct nx_timeval), "timeval public layout");
#endif
