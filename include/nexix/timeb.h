#ifndef NEXIX_TIMEB_H
#define NEXIX_TIMEB_H
#include <stdint.h>
/* Native syscall 35: explicit padding, signed 64-bit epoch seconds. */
struct nx_timeb { int64_t time; uint16_t millitm; int16_t timezone,dstflag; uint16_t reserved; };
_Static_assert(sizeof(struct nx_timeb)==16,"ftime ABI");
#endif
