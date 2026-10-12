#ifndef NEXIX_LIBC_SYS_STAT_H
#define NEXIX_LIBC_SYS_STAT_H
#include <sys/types.h>
#include <nexix/file.h>
/* Fixed native ABI; PDP-11 disk records are decoded by the kernel. */
struct stat {
    uint16_t st_dev, st_ino, st_mode, st_nlink, st_uid, st_gid, st_rdev, reserved;
    int64_t st_size, st_atime, st_mtime, st_ctime;
};
#define S_IREAD 0400
#define S_IWRITE 0200
#define S_IEXEC 0100
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFMPC 0030000
#define S_IFMPB 0070000
#define S_IFIFO 0010000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
/* mpx mode constants are source contracts; implementation is tasks 65–67. */
_Static_assert(sizeof(struct stat) == sizeof(struct nx_stat), "stat ABI size");
#define NX_STAT_OFFSET(field, native) \
    _Static_assert(offsetof(struct stat, field) == offsetof(struct nx_stat, native), "stat ABI offset")
NX_STAT_OFFSET(st_dev, dev); NX_STAT_OFFSET(st_ino, ino);
NX_STAT_OFFSET(st_mode, mode); NX_STAT_OFFSET(st_nlink, nlink);
NX_STAT_OFFSET(st_uid, uid); NX_STAT_OFFSET(st_gid, gid);
NX_STAT_OFFSET(st_rdev, rdev); NX_STAT_OFFSET(reserved, reserved);
NX_STAT_OFFSET(st_size, size); NX_STAT_OFFSET(st_atime, atime);
NX_STAT_OFFSET(st_mtime, mtime); NX_STAT_OFFSET(st_ctime, ctime);
#undef NX_STAT_OFFSET
int chmod(const char *, mode_t);
int chown(const char *, uid_t, gid_t);
mode_t umask(mode_t mask);
int stat(const char *path, struct stat *buffer);
int fstat(int fd, struct stat *buffer);
#endif
