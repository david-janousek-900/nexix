#ifndef NEXIX_LIBC_SGTTY_H
#define NEXIX_LIBC_SGTTY_H
#include <nexix/tty.h>
#include <stddef.h>
struct sgttyb { uint8_t sg_ispeed, sg_ospeed, sg_erase, sg_kill; uint32_t sg_flags; };
struct tchars { uint8_t t_intrc, t_quitc, t_startc, t_stopc, t_eofc, t_brkc; };
/* Values are exported even where the driver rejects unsupported modes.
 * Implementing these modes belongs to step 74. */
_Static_assert(sizeof(struct sgttyb) == sizeof(struct nx_sgttyb), "sgtty ABI size");
_Static_assert(offsetof(struct sgttyb, sg_flags) == offsetof(struct nx_sgttyb, flags), "sgtty ABI flags");
_Static_assert(sizeof(struct tchars) == sizeof(struct nx_tchars), "tchars ABI size");
#define TANDEM 01
#define LCASE 04
#define ANYP 0300
#define NLDELAY 001400
#define TBDELAY 006000
#define CRDELAY 030000
#define VTDELAY 040000
#define BSDELAY 0100000
#define ALLDELAY 0177400
#define CR0 0
#define CR1 010000
#define CR2 020000
#define CR3 030000
#define NL0 0
#define NL1 000400
#define NL2 001000
#define NL3 001400
#define TAB0 0
#define TAB1 002000
#define TAB2 004000
#define FF0 0
#define FF1 040000
#define BS0 0
#define BS1 0100000
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
