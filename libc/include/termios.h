#ifndef NEXIX_LIBC_TERMIOS_H
#define NEXIX_LIBC_TERMIOS_H
#include <stdint.h>
#include <sgtty.h>
typedef uint32_t tcflag_t;
typedef uint8_t cc_t;
typedef uint8_t speed_t;
#define NCCS 10
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VSTART 5
#define VSTOP 6
#define VMIN 7
#define VTIME 8
#define VEOL 9
#define ICRNL 1U
#define IUCLC 2U
#define IXON 4U
#define OPOST 1U
#define ONLCR 2U
#define TABDLY NX_TBDELAY
#define TAB3 NX_XTABS
#define CSIZE 3U
#define CS8 3U
#define CREAD 4U
#define PARENB 8U
#define PARODD 16U
/* ECHO is shared with the historical sgtty header. */
#define ISIG 1U
#define ICANON 2U
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2
#define _POSIX_VDISABLE 255
struct termios {
    tcflag_t c_iflag,c_oflag,c_cflag,c_lflag;
    cc_t c_cc[NCCS];
    speed_t c_ispeed,c_ospeed;
    uint32_t __native_flags;
};
int tcgetattr(int,struct termios *);
int tcsetattr(int,int,const struct termios *);
speed_t cfgetispeed(const struct termios *);
speed_t cfgetospeed(const struct termios *);
int cfsetispeed(struct termios *,speed_t);
int cfsetospeed(struct termios *,speed_t);
#endif
