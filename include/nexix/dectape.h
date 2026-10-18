#ifndef NEXIX_ABI_DECTAPE_H
#define NEXIX_ABI_DECTAPE_H
#include <nexix/tape.h>
/* Native media-loading extension; V7 tc has no character device or ioctl. */
#define NX_TAPE_TC 2U
#define NX_TC_MAJOR 22U
#define NX_TC_UNITS 8U
#define NX_TC_BLOCKS 578U
#define NX_TC_BYTES (NX_TC_BLOCKS*512U)
#define NX_TC_DEV(unit) (NX_TC_MAJOR*256U+(unit))
#endif
