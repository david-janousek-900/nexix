/* V7 libc/gen/pkon.c: native ioctl layout; pkoff still closes the descriptor. */
#include <sys/pk.h>
#include <unistd.h>
#include <errno.h>
static int valid(int size){return size>=32 && size<=NX_PK_MAX_SIZE && !(size&(size-1));}
int turnon(int fd,int size,int window,int mode)
{
    if(!valid(size) || window<1 || window>NX_PK_WINDOW || mode<0 || mode>1){errno=EINVAL;return -1;}
    struct piocb p={.t=1,.psize=size,.window=window,.mode=mode};
    if(ioctl(fd,TIOCSETD,&p)<0)return -1;
    return ioctl(fd,DIOCGETP,&p);
}
int pkon(int fd,int size)
{
    if(!valid(size) || size>128){(void)write(2,"bad packet size\n",16);errno=EINVAL;return -1;}
    return turnon(fd,size,2,0);
}
int pkoff(int fd){return close(fd);}
