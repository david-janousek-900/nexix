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
#define NX_MX_PORTS 30
#define NX_MX_INDEX 15
#define NX_MX_LEVELS 4
#define NX_MX_MAJOR 5
#define NX_MX_HIQ 100
#define NX_MX_LOQ 20
#define NX_MX_QSIZE 200
#define NX_MX_EVENTS 16
#define NX_MX_NAME 50
#define NX_M_WATCH 1
#define NX_M_CLOSE 2
#define NX_M_EOT 3
#define NX_M_OPEN 4
#define NX_M_BLK 5
#define NX_M_UBLK 6
#define NX_M_IOCTL 12
#define NX_M_SIG 14
#define NX_MXLSTN 0x7801
#define NX_MXNBLK 0x7802
/* Read records have a six-byte header, inline payload, and two-byte alignment.
 * Write records contain a native pointer; reserved must be zero. Headers are
 * writable: ccount=-1, count/data report a truncated record for retry. */
struct nx_mx_read { uint16_t index, count, ccount; };
struct nx_mx_write { uint16_t index, count; int16_t ccount; uint16_t reserved; const void *data; };
struct nx_mx_control { uint16_t code, value; };
struct nx_mx_ioctl { uint16_t code, command; uint8_t data[8]; };
_Static_assert(sizeof(struct nx_mx_ioctl)==12,"mpx ioctl ABI");
_Static_assert(sizeof(struct nx_mx_read)==6,"mpx read ABI");
_Static_assert(sizeof(struct nx_mx_write)==16,"mpx write ABI");
_Static_assert(sizeof(struct nx_mx_control)==4,"mpx control ABI");
struct nx_mx_args { const char *name; int32_t command; int32_t arg[3]; };
_Static_assert(sizeof(struct nx_mx_args)==24,"mpx argument ABI");
/* Syscall 56(command, &args). Indices are zero-extended 16-bit values:
 * negative long results remain reserved for native errno. V7 C wrappers are in <sys/mx.h>. */
#endif
