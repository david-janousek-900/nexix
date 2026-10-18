#ifndef NEXIX_ABI_TTY_H
#define NEXIX_ABI_TTY_H
/* V7 ioctl numbers and sgtty flags; records use the native ABI. */
#define NX_TIOCGETD 0x7400
#define NX_TIOCSETD 0x7401
#define NX_TIOCHPCL 0x7402
#define NX_TIOCEXCL 0x740d
#define NX_TIOCNXCL 0x740e
#define NX_TIOCGETP 0x7408
#define NX_TIOCSETP 0x7409
#define NX_TIOCSETN 0x740a
#define NX_TIOCFLUSH 0x7410
#define NX_TIOCSETC 0x7411
#define NX_TIOCGETC 0x7412
#define NX_FIOCLEX 0x6601
#define NX_FIONCLEX 0x6602
#define NX_TANDEM 0000001
#define NX_LCASE 0000004
#define NX_NLDELAY 001400
#define NX_TBDELAY 006000
#define NX_CRDELAY 030000
#define NX_VTDELAY 040000
#define NX_BSDELAY 0100000 /* Historical flag; V7 ttyoutput applies no BS delay. */
#define NX_CBREAK 0000002
#define NX_ECHO 0000010
#define NX_CRMOD 0000020
#define NX_RAW 0000040
#define NX_ODDP 0000100
#define NX_EVENP 0000200
#define NX_XTABS 0006000
#define NX_B0 0
#define NX_B50 1
#define NX_B75 2
#define NX_B110 3
#define NX_B134 4
#define NX_B150 5
#define NX_B200 6
#define NX_B300 7
#define NX_B600 8
#define NX_B1200 9
#define NX_B1800 10
#define NX_B2400 11
#define NX_B4800 12
#define NX_B9600 13
#define NX_EXTA 14
#define NX_EXTB 15
#define NX_B115200 16 /* Nexix extension; boot console rate */
#define NX_TTY_NATIVE 0U
#define NX_TTY_DC 1U
#define NX_TTY_DH 2U
#define NX_TTY_DZ 3U
#define NX_TTY_KL 4U
#define NX_TTY_CONTROLLER_DEV(profile,line) (((22U+(profile))*256U)+(line))
#define NX_RX_PARITY 1U
#define NX_RX_FRAME 2U
#define NX_RX_BREAK 4U
#define NX_RX_OVERRUN 8U
#define NX_CONSOLE_DEV 0x0100
#define NX_CTTY_DEV 0x0200
#ifndef __ASSEMBLER__
#include <stdint.h>
struct nx_sgttyb { uint8_t ispeed, ospeed, erase, kill; uint32_t flags; };
struct nx_tchars { uint8_t intr, quit, start, stop, eof, brk; };
_Static_assert(sizeof(struct nx_sgttyb) == 8, "native sgtty ABI");
_Static_assert(sizeof(struct nx_tchars) == 6, "native tchars ABI");
#endif
#endif
