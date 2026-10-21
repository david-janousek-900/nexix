#include <sys/mx.h>
#include <sgtty.h>
#include <sys/kinfo.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <nexix/output.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"mpx tty line %d errno %d\n",__LINE__,errno);return __LINE__%200+1;}}while(0)
static long inject(const void *p,size_t n)
{long r=0x919;__asm__ volatile("int $0x81":"+a"(r):"D"(p),"S"(n):"memory","cc");return r;}
static long opens(void)
{long r=0x91a;__asm__ volatile("int $0x81":"+a"(r)::"memory","cc");return r;}
static int finished(pid_t pid)
{int status;return wait(&status)==pid && WIFEXITED(status) && !WEXITSTATUS(status);}
static int sleeping(pid_t pid)
{
    struct {struct nx_kinfo_header h;struct nx_kinfo_process p[16];} s;
    for(unsigned tries=0;tries<2000;++tries){
        if(nx_kinfo(1,NX_KINFO_PROCESS,&s,sizeof(s))<0)return 0;
        for(unsigned i=0;i<s.h.count;++i)if(s.p[i].pid==pid){
            if(s.p[i].state==NX_KPROC_SLEEP)return 1;
            if(s.p[i].state==NX_KPROC_ZOMBIE)return 0;
        }
        sched_yield();
    }
    return 0;
}
static int send(int g,int id,const void *data,unsigned n)
{struct wh h={.index=id,.count=n,.data=(char *)data};return write(g,&h,sizeof(h));}
static int control(int g,int id,struct nx_mx_ioctl *message)
{struct wh h={.index=id,.ccount=sizeof(*message),.data=(char *)message};return write(g,&h,sizeof(h));}
static int event(int g,int id,int code,int value)
{
    uint8_t b[12];ssize_t n=read(g,b,sizeof(b));struct rh *h=(void *)b;
    struct nx_mx_control *e=(void *)(b+sizeof(*h));
    return n==10 && h->index==id && !h->count && h->ccount==4 && e->code==code && e->value==value;
}
static void handler(int sig){(void)sig;}
static int wrappers(void)
{
    int g=mpx("",0600);CHECK(g>=0);
    CHECK(mpx(NULL,0600)==-1 && errno==EFAULT);
    CHECK(mpxcall(CHAN,NULL)==-1 && errno==EFAULT);
    CHECK(chan(-1)==-1 && errno==ENXIO && debug(27,1)==0 && debug(27,0)==0);
    CHECK(debug(30,1)==-1 && errno==EINVAL);
    int sub=mpxcall(MPXN,NULL);CHECK(sub>=0 && join(sub,g)==0);
    int id=chan(sub);CHECK(id==0xff00);
    int e=extract(id,g,1);CHECK(e>=0 && attach(id,g)==id);
    CHECK(write(e,"W",1)==1);uint8_t b[12];CHECK(read(g,b,sizeof(b))==8 && b[6]=='W');
    CHECK(close(e)==0 && event(g,id,M_CLOSE,0) && detach(id,g)==0);
    CHECK(close(sub)==0 && detach(0xfff0,g)==0 && close(g)==0);
    g=mpx("/tmp/mx-wrapper",0600);CHECK(g>=0 && close(g)==0 && unlink("/tmp/mx-wrapper")==0);return 0;
}
static int ports(int t)
{
    int g=mpx("",0600);CHECK(g>=0);long before=opens();
    int id=join(t,g);CHECK(id==0xfff0);
    int other=mpx("",0600);CHECK(other>=0 && join(t,other)==-1 && errno==EBUSY && close(other)==0);
    int duplicate=extract(id,g,1);CHECK(duplicate>=0 && close(duplicate)==0);
    CHECK(opens()==before); /* The canonical TTY file is retained, not reopened. */
    CHECK(inject("ab\bC\r",5)==0);uint8_t b[128];
    CHECK(read(g,b,sizeof(b))==10 && ((struct rh *)b)->count==3 && !memcmp(b+6,"aC\n",3));
    CHECK(inject("\004",1)==0 && event(g,id,M_EOT,0));
    CHECK(inject("\003",1)==0 && event(g,id,M_SIG,SIGINT));
    CHECK(inject("\034",1)==0 && event(g,id,M_SIG,SIGQUIT));
    CHECK(ckill(id,g,SIGALRM)==SIGALRM); /* TTY ports have no process target. */
    CHECK(send(g,id,"NEXIX_MPX_TTY_OUTPUT\n",21)==16);
    struct nx_mx_ioctl request={.code=M_IOCTL,.command=TIOCGETP};
    CHECK(control(g,id,&request)==16 && read(g,b,sizeof(b))==18);
    struct nx_mx_ioctl reply;memcpy(&reply,b+6,sizeof(reply));CHECK(reply.code==M_IOCTL && reply.command==TIOCGETP);
    struct sgttyb settings;memcpy(&settings,reply.data,sizeof(settings));settings.sg_flags=RAW;
    request.command=TIOCSETP;memcpy(request.data,&settings,sizeof(settings));CHECK(control(g,id,&request)==16);
    CHECK(inject("\000\377\003",3)==0 && read(g,b,sizeof(b))==10 && !memcmp(b+6,"\000\377\003",3));
    settings.sg_flags=CRMOD;memcpy(request.data,&settings,sizeof(settings));CHECK(control(g,id,&request)==16);
    CHECK(detach(id,g)==0 && inject("ordinary\n",9)==0);
    CHECK(read(t,b,sizeof(b))==9 && !memcmp(b,"ordinary\n",9) && close(g)==0);
    return 0;
}
static int connections(int t)
{
    int g=mpx("",0600),id=chan(g),e=extract(id,g,1);CHECK(g>=0 && id>=0 && e>=0);
    int rd=open("/dev/console",O_RDONLY),wr=open("/dev/console",O_WRONLY);CHECK(rd>=0 && wr>=0);
    CHECK(connect(rd,e,1)==-1 && errno==EBADF && join(rd,g)==-1 && errno==EBADF);
    CHECK(connect(wr,e,-1)==-1 && errno==EBADF && join(wr,g)==-1 && errno==EBADF);
    CHECK(connect(rd,e,-1)==0 && disconnect(rd,e,-1)==0);
    CHECK(connect(wr,e,1)==0 && disconnect(wr,e,1)==0 && close(rd)==0 && close(wr)==0);
    CHECK(mknod("/tmp/mx-cat",S_IFCHR|0600,NX_CAT_DEV)==0);
    int out=open("/tmp/mx-cat",O_WRONLY);CHECK(out>=0);
    CHECK(connect(out,e,1)==-1 && errno==ENXIO && join(out,g)==-1 && errno==ENXIO);
    CHECK(close(out)==0 && unlink("/tmp/mx-cat")==0);
    CHECK(connect(t,e,-1)==0 && inject("input\n",6)==0);uint8_t b[128];
    CHECK(read(g,b,sizeof(b))==12 && !memcmp(b+6,"input\n",6));
    CHECK(send(g,id,"pipe",4)==16 && read(e,b,sizeof(b))==4 && !memcmp(b,"pipe",4));
    CHECK(disconnect(t,e,-1)==0 && connect(t,e,1)==0);
    CHECK(write(e,"queue",5)==5 && read(g,b,sizeof(b))==12 && !memcmp(b+6,"queue",5));
    CHECK(inject("plain\n",6)==0 && read(t,b,sizeof(b))==6 && !memcmp(b,"plain\n",6));
    CHECK(send(g,id,"NEXIX_MPX_CONNECT_OUTPUT\n",25)==16);
    CHECK(disconnect(t,e,1)==0 && disconnect(t,e,1)==-1 && errno==ENXIO);
    CHECK(connect(t,e,0)==0);
    int d=open("/dev/console",O_RDWR);CHECK(d>=0 && connect(d,e,0)==0 && close(d)==0);
    /* Forwarded endpoint ioctls also notify the group owner. */
    struct sgttyb settings;CHECK(gtty(e,&settings)==0 && read(g,b,sizeof(b))==18);
    CHECK(((struct nx_mx_ioctl *)(b+6))->command==TIOCGETP);
    CHECK(disconnect(t,e,0)==0 && send(g,id,"again",5)==16 && read(e,b,sizeof(b))==5);
    CHECK(close(e)==0 && close(g)==0);return 0;
}
static int lifetime(void)
{
    long before=opens();int t=open("/dev/console",O_RDWR),g=mpx("",0600);CHECK(t>=0 && g>=0);
    int id=join(t,g);CHECK(id>=0 && close(t)==0 && opens()==before+1);
    int d=extract(id,g,1);CHECK(d>=0 && inject("owned\n",6)==0);
    uint8_t b[64];CHECK(read(g,b,sizeof(b))==12 && !memcmp(b+6,"owned\n",6));
    CHECK(close(d)==0 && detach(id,g)==0 && opens()==before && close(g)==0);
    t=open("/dev/console",O_RDWR);g=mpx("",0600);id=chan(g);int e=extract(id,g,1);
    CHECK(t>=0 && e>=0 && connect(t,e,0)==0 && close(t)==0 && opens()==before+1);
    CHECK(inject("kept\n",5)==0 && read(g,b,sizeof(b))==12 && !memcmp(b+6,"kept\n",5));
    CHECK(close(g)==0 && opens()==before && close(e)==0);
    return 0;
}
static int flow(int t)
{
    int g=mpx("",0600),id=chan(g),e=extract(id,g,1);CHECK(e>=0 && connect(t,e,1)==0);
    CHECK(ioctl(g,MXNBLK,0)==0 && inject("\023",1)==0);
    struct wh h={.index=id,.count=1,.data="F"};
    CHECK(write(g,&h,sizeof(h))==15 && h.count==1 && h.ccount==-1);
    CHECK(inject("\021",1)==0 && event(g,id,M_UBLK,0));h.ccount=0;
    CHECK(write(g,&h,sizeof(h))==16);
    CHECK(close(e)==0 && close(g)==0);
    g=mpx("",0600);id=chan(g);e=extract(id,g,1);CHECK(e>=0 && connect(t,e,1)==0 && inject("\023",1)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){CHECK(send(g,id,"B",1)==16);_exit(0);}
    CHECK(sleeping(pid) && inject("\021",1)==0 && finished(pid));
    CHECK(inject("\023",1)==0);pid=fork();CHECK(pid>=0);
    if(!pid){CHECK(send(g,id,"old",3)==-1 && errno==ENXIO);_exit(0);}
    CHECK(sleeping(pid) && disconnect(t,e,1)==0 && finished(pid) && inject("\021",1)==0);
    CHECK(close(e)==0 && close(g)==0);return 0;
}
static int virtual_ioctl(int cancel)
{
    int g=mpx("",0600),id=chan(g),e=extract(id,g,1);CHECK(e>=0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        signal(SIGALRM,handler);struct sgttyb settings;
        int result=gtty(e,&settings);
        if(cancel)CHECK(result==-1 && errno==EINTR);
        else CHECK(result==0 && settings.sg_erase==8 && settings.sg_kill==21);
        _exit(0);
    }
    CHECK(sleeping(pid));uint8_t b[64];CHECK(read(g,b,sizeof(b))==18);
    struct nx_mx_ioctl response;memcpy(&response,b+6,sizeof(response));
    CHECK(response.code==M_IOCTL && response.command==TIOCGETP);
    if(cancel)CHECK(kill(pid,SIGALRM)==0 && finished(pid));
    struct sgttyb settings={.sg_erase=8,.sg_kill=21};memcpy(response.data,&settings,sizeof(settings));
    if(cancel==1)CHECK(control(g,id,&response)==-1 && errno==EINTR);
    else if(cancel==2){
        CHECK(close(g)==0 && gtty(e,&settings)==-1 && errno==ENXIO && close(e)==0);return 0;
    }
    else CHECK(control(g,id,&response)==16 && finished(pid));
    CHECK(close(e)==0 && close(g)==0);return 0;
}
static int concurrent_ioctls(void)
{
    int g=mpx("",0600),id=chan(g),e=extract(id,g,1);CHECK(e>=0);
    pid_t children[2];uint8_t b[64];
    for(int i=0;i<2;++i){
        children[i]=fork();CHECK(children[i]>=0);
        if(!children[i]){struct sgttyb settings;CHECK(gtty(e,&settings)==0 && settings.sg_erase=='A'+i);_exit(0);}
        CHECK(sleeping(children[i]));
        if(i==0)CHECK(read(g,b,sizeof(b))==18);
    }
    for(int i=0;i<2;++i){
        struct nx_mx_ioctl response={.code=M_IOCTL,.command=TIOCGETP};
        struct sgttyb settings={.sg_erase='A'+i};memcpy(response.data,&settings,sizeof(settings));
        CHECK(control(g,id,&response)==16);
        if(i==0)CHECK(read(g,b,sizeof(b))==18);
    }
    CHECK(finished(children[0]) && finished(children[1]) && close(e)==0 && close(g)==0);return 0;
}
static int groups(void)
{
    int g=mpx("",0600),id=chan(g),e=extract(id,g,1);CHECK(e>=0);
    pid_t original=getpgrp();CHECK(npgrp(-1,-1,0)==getpid() && npgrp(-1,-1,original)==original);
    CHECK(ckill(id,g,0)==-1 && errno==EINVAL && npgrp(id,g,-1)==-1 && errno==EINVAL);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){signal(SIGALRM,handler);CHECK(npgrp(id,g,0)==getpid());CHECK(pause()==-1 && errno==EINTR);_exit(0);}
    CHECK(sleeping(pid));
    pid_t attacker=fork();CHECK(attacker>=0);
    if(!attacker){
        CHECK(npgrp(id,g,pid)==pid && npgrp(-1,-1,0)==getpid() && setuid(123)==0);
        CHECK(ckill(id,g,SIGALRM)==-1 && errno==EPERM);_exit(0);
    }
    CHECK(finished(attacker) && sleeping(pid));
    CHECK(ckill(id,g,SIGALRM)==SIGALRM && finished(pid));
    CHECK(close(e)==0 && close(g)==0);return 0;
}
int mpx_tty_test(void)
{
    ssize_t files=nx_kinfo(1,NX_KINFO_FILE,0,0),inodes=nx_kinfo(1,NX_KINFO_INODE,0,0);long before=opens();
    int t=open("/dev/console",O_RDWR);CHECK(t>=0);struct sgttyb original,settings;
    CHECK(gtty(t,&original)==0);settings=original;settings.sg_flags=CRMOD;CHECK(stty(t,&settings)==0);
    int error=wrappers();if(error)return error;
    error=ports(t);if(error)return error;
    error=connections(t);if(error)return error;
    error=lifetime();if(error)return error;
    error=flow(t);if(error)return error;
    error=virtual_ioctl(0);if(error)return error;
    error=virtual_ioctl(1);if(error)return error;
    error=virtual_ioctl(2);if(error)return error;
    error=concurrent_ioctls();if(error)return error;
    error=groups();if(error)return error;
    CHECK(stty(t,&original)==0 && close(t)==0 && opens()==before);
    CHECK(nx_kinfo(1,NX_KINFO_FILE,0,0)==files && nx_kinfo(1,NX_KINFO_INODE,0,0)==inodes);
    const char marker[]="NEXIX_MPX_TTY_OK\n";CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);return 0;
}
