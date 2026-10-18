#ifndef NEXIX_LIBC_SYS_IOCTL_H
#define NEXIX_LIBC_SYS_IOCTL_H
#include <nexix/tty.h>
#include <nexix/packet.h>
#define DIOCGETP NX_DIOCGETP
#include <nexix/mpx.h>
#define MXLSTN NX_MXLSTN
#define MXNBLK NX_MXNBLK
#define TIOCGETD NX_TIOCGETD
#define TIOCSETD NX_TIOCSETD
#define TIOCHPCL NX_TIOCHPCL
#define TIOCEXCL NX_TIOCEXCL
#define TIOCNXCL NX_TIOCNXCL
#define TIOCGETP NX_TIOCGETP
#define TIOCSETP NX_TIOCSETP
#define TIOCSETN NX_TIOCSETN
#define TIOCFLUSH NX_TIOCFLUSH
#define TIOCSETC NX_TIOCSETC
#define TIOCGETC NX_TIOCGETC
#define FIOCLEX NX_FIOCLEX
#define FIONCLEX NX_FIONCLEX
int ioctl(int fd, unsigned long request, void *argument);
#endif
