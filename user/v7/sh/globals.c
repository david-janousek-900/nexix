/* Definitions formerly supplied by common declarations in V7 headers. */
#include "defs.h"
INT		ioset;
IOPTR		iotemp;		/* files to be deleted sometime */
IOPTR		iopend;		/* documents waiting to be read at NL */
INT		dolc;
DOLPTR		argfor;
ARGPTR		gchain;
INT		wdval;
INT		wdnum;
ARGPTR		wdarg;
INT		wdset;
BOOL		reserv;
STRING		cmdadr;
STRING		exitadr;
STRING		dolladr;
STRING		pcsadr;
STRING		pidadr;
STRING		tmpnam;
INT		serial;
INT		peekc;
STRING		comdiv;
INT		flags;
jmp_buf		subshell;
jmp_buf		errshell;
BOOL		trapnote;
INT		exitval;
BOOL		execbrk;
INT		loopcnt;
INT		breakcnt;
BLKPTR heapbase;
BLKPTR		stakbsy;
STKPTR		stakbas;
STKPTR		brkend;
STKPTR		staktop;
