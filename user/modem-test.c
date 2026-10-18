#include <sys/ioctl.h>
#include <sys/mx.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c))return __LINE__%240+1;}while(0)
static int peer(int fd,unsigned dev,unsigned op,unsigned bits)
{struct nx_modem_peer p={.device=dev,.operation=op,.signals=bits};return ioctl(fd,NX_MDM_PEER,&p);}
static int finished(pid_t pid,int expected)
{int status;return wait(&status)==pid && WIFEXITED(status) && WEXITSTATUS(status)==expected;}
static int await(int fd,struct nx_modem_peer *p,unsigned state)
{
    struct tms accounting;clock_t start=times(&accounting);
    while(times(&accounting)-start<8*NX_CLK_TCK){p->operation=NX_PEER_STATUS;p->count=0;if(ioctl(fd,NX_MDM_PEER,p))return -1;
        if(p->state&state)return 0;
        sched_yield();
    }
    return -1;
}
static void hangup(int sig){(void)sig;_exit(42);}
int modem_test(void)
{
    int ctl=open("/dev/modemctl",O_RDWR);CHECK(ctl>=0);
    struct nx_modem_peer p={.device=NX_DN_DEV(0)};
    CHECK(peer(ctl,p.device,NX_PEER_SIGNALS,NX_DN_READY)==0);
    int dn=open("/dev/dn0",O_WRONLY);CHECK(dn>=0 && open("/dev/dn0",O_WRONLY)==-1 && errno==ENXIO);
    int group=mpx("",0600);CHECK(group>=0 && join(dn,group)==-1 && errno==ENXIO && close(group)==0);
    int duplicate=dup(dn);CHECK(duplicate>=0 && close(dn)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){close(ctl);int ok=write(duplicate,"1-2",3)==3;close(duplicate);_exit(ok?0:1);}
    CHECK(close(duplicate)==0);
    for(unsigned i=0;i<2;++i){
        CHECK(await(ctl,&p,NX_MODEM_PENDING)==0);p.operation=NX_PEER_TAKE;p.count=1;
        CHECK(ioctl(ctl,NX_MDM_PEER,&p)==0 && p.count==1 && p.data[0]==(i?'2':'1'));
        CHECK(peer(ctl,p.device,NX_PEER_SIGNALS,i?NX_DN_CONNECTED:NX_DN_READY)==0);
    }
    CHECK(finished(child,0));p.operation=NX_PEER_STATUS;p.count=0;
    CHECK(ioctl(ctl,NX_MDM_PEER,&p)==0 && !(p.state&NX_MODEM_OPEN));
    CHECK(peer(ctl,NX_DU_DEV,NX_PEER_SIGNALS,NX_MDM_DSR|NX_MDM_CTS|NX_MDM_DCD)==0);
    int du=open("/dev/du0",O_RDWR);CHECK(du>=0);
    child=fork();CHECK(child>=0);
    if(!child){
        close(ctl);unsigned char bytes[3];
        int ok=open("/dev/du0",O_RDWR)==-1 && errno==ENXIO && read(du,bytes,3)==3 &&
            !memcmp(bytes,"a\200z",3) && write(du,"sync",4)==4;
        close(du);_exit(ok?0:2);
    }
    p=(struct nx_modem_peer){.device=NX_DU_DEV,.operation=NX_PEER_RECEIVE,.count=3};
    memcpy(p.data,"a\200z",3);CHECK(ioctl(ctl,NX_MDM_PEER,&p)==0);
    CHECK(peer(ctl,NX_DU_DEV,NX_PEER_END,0)==0 && await(ctl,&p,NX_MODEM_WRITE)==0);
    p.operation=NX_PEER_TAKE;p.count=512;CHECK(ioctl(ctl,NX_MDM_PEER,&p)==0 && p.count==4 && !memcmp(p.data,"sync",4));
    CHECK(finished(child,0) && close(du)==0);
    child=fork();CHECK(child>=0);
    if(!child){struct nx_modem_peer q={.device=NX_DU_DEV};int ok=setuid(1)==0 && ioctl(ctl,NX_MDM_PEER,&q)==-1 && errno==EPERM;_exit(ok?0:3);}
    CHECK(finished(child,0));
    struct nx_modem_tty status;CHECK(ioctl(0,NX_MDM_GET,&status)==0 && status.mode==NX_MDM_LOCAL && status.outputs==(NX_MDM_DTR|NX_MDM_RTS));
    struct nx_modem_tty mode={.mode=NX_MDM_VIRTUAL};CHECK(ioctl(0,NX_MDM_SET,&mode)==0);
    CHECK(peer(ctl,NX_CONSOLE_DEV,NX_PEER_TTY,NX_MDM_DCD)==0);
    int ready[2];CHECK(pipe(ready)==0);child=fork();CHECK(child>=0);
    if(!child){close(ready[0]);signal(SIGHUP,hangup);int ok=setpgrp()==getpid() && write(ready[1],"r",1)==1;close(ready[1]);if(!ok)_exit(4);pause();_exit(5);}
    CHECK(close(ready[1])==0);char byte;CHECK(read(ready[0],&byte,1)==1 && close(ready[0])==0);
    CHECK(peer(ctl,NX_CONSOLE_DEV,NX_PEER_TTY,0)==0 && finished(child,42));
    mode.mode=NX_MDM_LOCAL;CHECK(ioctl(0,NX_MDM_SET,&mode)==0 && setpgrp()==getpid());
    CHECK(close(ctl)==0);puts("NEXIX_MODEM_OK");return 0;
}
