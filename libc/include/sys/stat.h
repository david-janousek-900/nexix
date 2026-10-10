#ifndef NEXIX_LIBC_SYS_STAT_H
#define NEXIX_LIBC_SYS_STAT_H
#include <sys/types.h>
/* Fixed native ABI; PDP-11 disk records are decoded by the kernel. */
struct stat {
    uint16_t st_dev, st_ino, st_mode, st_nlink, st_uid, st_gid, st_rdev, reserved;
    int64_t st_size, st_atime, st_mtime, st_ctime;
};
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFIFO 0010000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
int chmod(const char *, mode_t);
int chown(const char *, uid_t, gid_t);
mode_t umask(mode_t mask);
int stat(const char *path, struct stat *buffer);
int fstat(int fd, struct stat *buffer);
#endif
