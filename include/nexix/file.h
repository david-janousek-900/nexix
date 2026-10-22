#ifndef NEXIX_ABI_FILE_H
#define NEXIX_ABI_FILE_H
#define NX_O_RDONLY 0
#define NX_O_WRONLY 1
#define NX_O_RDWR 2
#define NX_O_ACCMODE 3
#define NX_O_NONBLOCK 0x4   /* BSD value; sockets only (task 426) */
#define NX_O_CREAT 0x100
#define NX_O_TRUNC 0x200
#define NX_O_EXCL 0x400
#define NX_O_APPEND 0x800
#define NX_O_CLOEXEC 0x1000
#define NX_F_GETFD 1
#define NX_F_SETFD 2
#define NX_F_GETFL 3
#define NX_F_SETFL 4
#define NX_FD_CLOEXEC 1
#define NX_SEEK_SET 0
#define NX_SEEK_CUR 1
#define NX_SEEK_END 2
#define NX_PIPE_BUF 4096
#define NX_BLOCK_DISK_MAJOR 0
#define NX_RAW_DISK_MAJOR 4
#define NX_ROOT_BLOCK_DEV 0x0000
#define NX_ROOT_RAW_DEV 0x0400
#define NX_NULL_DEV 0x0300
#ifndef __ASSEMBLER__
#include <stdint.h>
struct nx_stat {
    uint16_t dev, ino, mode, nlink, uid, gid, rdev, reserved;
    int64_t size, atime, mtime, ctime;
};
_Static_assert(sizeof(struct nx_stat) == 48, "stat ABI");
#endif
#endif
