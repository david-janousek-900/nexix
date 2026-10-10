#ifndef NEXIX_V7_TYPES_H
#define NEXIX_V7_TYPES_H
#include <stdint.h>
#include <stddef.h>
/* Runtime types, not disk encodings or a published user ABI. */
typedef int64_t v7_off_t;
typedef int64_t v7_time_t;
typedef int32_t v7_daddr_t;
typedef uint16_t v7_ino_t;
typedef uint16_t v7_dev_t;
typedef uint16_t v7_uid_t;
typedef int32_t v7_pid_t;
#define V7_NODEV UINT16_MAX
static inline unsigned v7_major(v7_dev_t dev) { return dev >> 8; }
static inline unsigned v7_minor(v7_dev_t dev) { return dev & 255; }
/* Callers must validate both components <= 255. */
static inline v7_dev_t v7_makedev(uint8_t major, uint8_t minor)
{ return (v7_dev_t)((uint16_t)major << 8 | minor); }
_Static_assert(sizeof(uintptr_t) == 8, "Nexix requires 64-bit pointers");
_Static_assert(sizeof(v7_off_t) == 8, "native offsets must not truncate");
#endif
