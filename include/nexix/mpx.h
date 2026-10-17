#ifndef NEXIX_ABI_MPX_H
#define NEXIX_ABI_MPX_H
#include <stdint.h>
/* V7 mpx command numbers; native pointers and signed 32-bit arguments. */
#define NX_MX_CHAN 1
#define NX_MX_JOIN 2
#define NX_MX_EXTR 3
#define NX_MX_ATTACH 4
#define NX_MX_MPX 5
#define NX_MX_MPXN 6
#define NX_MX_CONNECT 7
#define NX_MX_DETACH 8
#define NX_MX_DISCON 9
#define NX_MX_DEBUG 10
#define NX_MX_NPGRP 11
#define NX_MX_CSIG 12
#define NX_MX_PACK 13
#define NX_MX_GROUPS 10
#define NX_MX_CHANNELS 20
#define NX_MX_INDEX 15
#define NX_MX_LEVELS 4
#define NX_MX_MAJOR 5
struct nx_mx_args { const char *name; int32_t command; int32_t arg[3]; };
_Static_assert(sizeof(struct nx_mx_args)==24,"mpx argument ABI");
/* Syscall 56(command, &args). Indices are zero-extended 16-bit values:
 * negative long results remain reserved for native errno. Wrappers: task 67. */
#endif
