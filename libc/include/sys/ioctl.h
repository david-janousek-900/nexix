#ifndef NEXIX_LIBC_SYS_IOCTL_H
#define NEXIX_LIBC_SYS_IOCTL_H
#include <nexix/tty.h>
#define TIOCGETD NX_TIOCGETD
#define TIOCSETD NX_TIOCSETD
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
