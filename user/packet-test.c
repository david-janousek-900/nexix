/* Exchanges V7 g-protocol frames with tools/packet_smoke.py over real COM1. */
#include <unistd.h>
#include <fcntl.h>
#include <sgtty.h>
#include <sys/pk.h>
#include <sys/mx.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#define REQUIRE(c) do {if(!(c))return __LINE__;}while(0)
int main(void)
{
    unsigned char bytes[200];struct sgttyb saved,raw;int d;
    REQUIRE(open("/dev/console",O_RDWR)==0 && dup(0)==1);
    REQUIRE(gtty(0,&saved)==0);raw=saved;raw.sg_flags=RAW;REQUIRE(stty(0,&raw)==0);
    REQUIRE(pkon(0,0)==-1 && errno==EINVAL);
    REQUIRE(pkon(0,96)==-1 && errno==EINVAL);
    REQUIRE(pkon(0,256)==-1 && errno==EINVAL);
    REQUIRE(write(1,"NEXIX_PACKET_READY1\r\n",21)==21);
    REQUIRE(pkon(0,128)==64);
    REQUIRE(ioctl(0,TIOCGETD,&d)==0 && d==1);
    struct piocb p;REQUIRE(ioctl(0,DIOCGETP,&p)==64 && p.psize==64 && p.window==2);
    REQUIRE(stty(0,&saved)==-1 && errno==EBUSY);
    for(unsigned i=0;i<150;++i)bytes[i]=(unsigned char)(i*37);
    REQUIRE(write(1,bytes,150)==150);
    unsigned done=0;
    while(done<137){ssize_t n=read(0,bytes+done,137-done);REQUIRE(n>0);done+=n;}
    for(unsigned i=0;i<137;++i)REQUIRE(bytes[i]==(unsigned char)(i*19+7));
    REQUIRE(read(0,bytes,1)==0);
    REQUIRE(write(1,bytes,0)==0);
    REQUIRE(read(0,bytes,1)==0);
    REQUIRE(signal(SIGPIPE,SIG_IGN)==SIG_DFL);
    REQUIRE(write(1,"x",1)==-1 && errno==EPIPE);
    d=0;REQUIRE(ioctl(0,TIOCSETD,&d)==0);
    REQUIRE(write(1,"NEXIX_PACKET_READY2\r\n",21)==21);
    REQUIRE(turnon(0,512,2,0)==64);
    REQUIRE(write(1,"B",1)==1);
    REQUIRE(read(0,bytes,sizeof(bytes))==6 && !memcmp(bytes,"second",6));
    /* pkoff closes only its descriptor; the shared terminal stays attached. */
    REQUIRE(pkoff(1)==0);
    REQUIRE(write(1,"x",1)==-1 && errno==EBADF);
    REQUIRE(ioctl(0,TIOCGETD,&d)==0 && d==1);
    d=0;REQUIRE(ioctl(0,TIOCSETD,&d)==0);
    REQUIRE(ioctl(0,DIOCGETP,&p)==-1 && errno==ENXIO);
    REQUIRE(write(0,"NEXIX_PACKET_READY3\r\n",21)==21);
    REQUIRE(turnon(0,32,1,0)==64);
    int group=mpx("",0600);REQUIRE(group==1);int id=join(0,group);REQUIRE(id>=0);
    REQUIRE(read(0,bytes,1)==-1 && errno==ENXIO);
    struct wh w={.index=id,.count=1,.data="C"};REQUIRE(write(group,&w,sizeof(w))==sizeof(w));
    REQUIRE(read(group,bytes,sizeof(bytes))==10 && ((struct rh *)bytes)->count==4 && !memcmp(bytes+6,"port",4));
    REQUIRE(read(group,bytes,sizeof(bytes))==10 && ((struct rh *)bytes)->ccount==4 &&
        ((struct nx_mx_control *)(bytes+6))->code==M_EOT);
    w.count=5;w.data="DRAIN";REQUIRE(write(group,&w,sizeof(w))==sizeof(w));
    REQUIRE(close(0)==0); /* Port owns the terminal until detach performs last close. */
    REQUIRE(detach(id,group)==0 && close(group)==0);
    REQUIRE(open("/dev/console",O_RDWR)==0);
    REQUIRE(write(0,"NEXIX_PACKET_OK\n",16)==16);
    REQUIRE(close(0)==0);
    return 0;
}
