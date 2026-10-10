#ifndef NEXIX_LIBC_SYS_TIMES_H
#define NEXIX_LIBC_SYS_TIMES_H
#include <sys/types.h>
#define CLK_TCK NX_CLK_TCK
struct tms { clock_t tms_utime, tms_stime, tms_cutime, tms_cstime; };
clock_t times(struct tms *buffer);
_Static_assert(sizeof(struct tms) == sizeof(struct nx_tms), "times layout");
_Static_assert(offsetof(struct tms, tms_cstime) == offsetof(struct nx_tms, cstime), "times offsets");
#endif
