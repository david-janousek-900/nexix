#ifndef NEXIX_LIBC_SYS_TYPES_H
#define NEXIX_LIBC_SYS_TYPES_H
#include <nexix/syscall.h>
typedef nx_pid_t pid_t;
typedef nx_uid_t uid_t;
typedef nx_gid_t gid_t;
typedef nx_mode_t mode_t;
typedef nx_off_t off_t;
typedef nx_time_t time_t;
typedef nx_clock_t clock_t;
typedef int64_t ssize_t;
/* Device/inode identifiers retain V7's 16-bit encoding. Disk block numbers
 * remain signed 32-bit; pointers, file offsets and timestamps are native. */
typedef uint16_t dev_t;
typedef uint16_t ino_t;
typedef int32_t daddr_t;
typedef char *caddr_t;
#define major(d) ((int)(((unsigned)(dev_t)(d) >> 8) & 255))
#define minor(d) ((int)((unsigned)(dev_t)(d) & 255))
#define makedev(ma, mi) ((dev_t)(((unsigned)(ma) << 8) | (unsigned)(mi)))
#endif
