#ifndef NEXIX_LIBC_SGTTY_H
#define NEXIX_LIBC_SGTTY_H
#include <nexix/tty.h>
struct sgttyb { uint8_t sg_ispeed, sg_ospeed, sg_erase, sg_kill; uint32_t sg_flags; };
struct tchars { uint8_t t_intrc, t_quitc, t_startc, t_stopc, t_eofc, t_brkc; };
#define CBREAK NX_CBREAK
#define ECHO NX_ECHO
#define CRMOD NX_CRMOD
#define RAW NX_RAW
#define ODDP NX_ODDP
#define EVENP NX_EVENP
#define XTABS NX_XTABS
#define B0 NX_B0
#define B50 NX_B50
#define B75 NX_B75
#define B110 NX_B110
#define B134 NX_B134
#define B150 NX_B150
#define B200 NX_B200
#define B300 NX_B300
#define B600 NX_B600
#define B1200 NX_B1200
#define B1800 NX_B1800
#define B2400 NX_B2400
#define B4800 NX_B4800
#define B9600 NX_B9600
#define EXTA NX_EXTA
#define EXTB NX_EXTB
#define B115200 NX_B115200
int stty(int fd, const struct sgttyb *settings);
int gtty(int fd, struct sgttyb *settings);
#include <sys/ioctl.h>
#endif
