#ifndef NEXIX_LIBC_SYS_PARAM_H
#define NEXIX_LIBC_SYS_PARAM_H
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#define HZ NX_CLK_TCK
#define NBPW sizeof(int)
#define BSIZE 512
#define BMASK 0777
#define BSHIFT 9
#define NINDIR 128
#define NMASK 0177
#define NSHIFT 7
#define NICINOD 100
#define NICFREE 50
#define NODEV ((dev_t)-1)
#define ROOTINO ((ino_t)2)
#define SUPERB ((daddr_t)1)
#define itod(i) ((daddr_t)(((unsigned)(ino_t)(i) + 15) >> 3))
#define itoo(i) ((int)(((unsigned)(ino_t)(i) + 15) & 7))
/* No PDP-11 click/segment conversion or kernel table addresses are exported. */
#endif
