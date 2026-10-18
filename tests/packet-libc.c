#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <nexix/packet.h>
#include <nexix/tty.h>
#include <nexix/errno.h>
int pkon(int,int);int pkoff(int);int turnon(int,int,int,int);
int nx_packet_errno;
static unsigned calls,closes,diagnostics;
static int failure;
static int size=128,window=2;
int nx_packet_ioctl(int fd,unsigned long request,void *arg)
{
    assert(fd==7);struct nx_piocb *p=arg;++calls;
    if(failure==(int)calls){nx_packet_errno=NX_EINTR;return -1;}
    if(calls==1){assert(request==NX_TIOCSETD && p->discipline==1 && p->psize==size && p->window==window && !p->reserved);return 0;}
    assert(calls==2 && request==NX_DIOCGETP);p->psize=64;return 64;
}
int nx_packet_close(int fd){++closes;assert(fd==7 || fd==-1);if(fd<0){nx_packet_errno=NX_EBADF;return -1;}return 0;}
int64_t nx_packet_write(int fd,const void *bytes,size_t n)
{assert(fd==2 && n==16 && !memcmp(bytes,"bad packet size\n",16));++diagnostics;return n;}
int main(void)
{
    assert(pkon(7,128)==64 && calls==2 && !closes);calls=0;
    size=512;window=1;assert(turnon(7,size,window,0)==64 && calls==2);calls=0;
    assert(pkon(7,256)==-1 && nx_packet_errno==NX_EINVAL && !calls && diagnostics==1);
    assert(pkon(7,96)==-1 && nx_packet_errno==NX_EINVAL && diagnostics==2);
    assert(pkon(7,0)==-1 && nx_packet_errno==NX_EINVAL && diagnostics==3);
    assert(turnon(7,512,0,0)==-1 && nx_packet_errno==NX_EINVAL && !calls);
    assert(turnon(7,512,2,2)==-1 && nx_packet_errno==NX_EINVAL && !calls);
    size=128;window=2;failure=1;assert(pkon(7,128)==-1 && nx_packet_errno==NX_EINTR && calls==1);calls=0;
    failure=2;assert(pkon(7,128)==-1 && nx_packet_errno==NX_EINTR && calls==2);
    assert(!closes && pkoff(7)==0 && closes==1);
    assert(pkoff(-1)==-1 && nx_packet_errno==NX_EBADF && closes==2);
    puts("PASS: libc packet validation, ioctl negotiation/errors and pkoff close-only semantics");
}
