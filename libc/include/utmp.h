#ifndef NEXIX_LIBC_UTMP_H
#define NEXIX_LIBC_UTMP_H
#include <sys/types.h>
struct utmp { char ut_line[8], ut_name[8]; time_t ut_time; };
_Static_assert(sizeof(struct utmp)==24,"native utmp record");
#endif
