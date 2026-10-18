#ifndef NEXIX_LIBC_SYS_PK_H
#define NEXIX_LIBC_SYS_PK_H
#include <nexix/packet.h>
#include <stddef.h>
#include <sys/ioctl.h>
struct piocb { uint32_t t; int16_t psize,mode,state; uint8_t window,reserved; };
_Static_assert(sizeof(struct piocb)==sizeof(struct nx_piocb),"piocb size");
_Static_assert(offsetof(struct piocb,window)==offsetof(struct nx_piocb,window),"piocb window");
int pkon(int fd,int size);
int pkoff(int fd);
int turnon(int fd,int size,int window,int mode);
#endif
