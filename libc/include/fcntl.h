#ifndef NEXIX_LIBC_FCNTL_H
#define NEXIX_LIBC_FCNTL_H
#include <sys/types.h>
#include <nexix/file.h>
#define O_RDONLY NX_O_RDONLY
#define O_WRONLY NX_O_WRONLY
#define O_RDWR NX_O_RDWR
#define O_ACCMODE NX_O_ACCMODE
#define O_CREAT NX_O_CREAT
#define O_TRUNC NX_O_TRUNC
#define O_EXCL NX_O_EXCL
#define O_APPEND NX_O_APPEND
#define O_NONBLOCK NX_O_NONBLOCK   /* sockets only */
#define O_CLOEXEC NX_O_CLOEXEC
#define F_GETFD NX_F_GETFD
#define F_SETFD NX_F_SETFD
#define F_GETFL NX_F_GETFL
#define F_SETFL NX_F_SETFL
#define FD_CLOEXEC NX_FD_CLOEXEC
int open(const char *path, int flags, ...);
int creat(const char *path, mode_t mode);
int fcntl(int fd, int command, ...);
#endif
