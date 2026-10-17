#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/kinfo.h>
#include <signal.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <nexix/libc.h>
#include <nexix/mpx.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"mpx data line %d errno %d\n",__LINE__,errno);return __LINE__%200+1;}}while(0)
static int mx(int cmd,int a,int b)
{
    struct nx_mx_args v={.arg={a,b,1}};
    long r=nx_syscall6(NX_SYS_MPX,cmd,(long)&v,0,0,0,0);if(r<0){errno=-r;return -1;}return r;
}
static int named(const char *path)
{
    struct nx_mx_args v={.name=path,.arg={0666,0,0}};
    long r=nx_syscall6(NX_SYS_MPX,NX_MX_MPX,(long)&v,0,0,0,0);if(r<0){errno=-r;return -1;}return r;
}
static int pair(int *g,int *id,int *e)
{
    *g=mx(NX_MX_MPXN,0,0);if(*g<0)return -1;
    *id=mx(NX_MX_CHAN,0,*g);if(*id<0)return -1;
    *e=mx(NX_MX_EXTR,*id,*g);return *e<0?-1:0;
}
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
static int send(int g,int id,const void *p,unsigned n)
{struct nx_mx_write h={.index=id,.count=n,.data=p};return write(g,&h,sizeof(h));}
static int eot(int g,int id)
{
    struct nx_mx_control e={NX_M_EOT,0};struct nx_mx_write h={.index=id,.ccount=4,.data=&e};
    return write(g,&h,sizeof(h));
}
static int event(int g,unsigned code,unsigned id)
{
    uint8_t b[12];ssize_t n=read(g,b,sizeof(b));
    struct nx_mx_read *h=(void *)b;struct nx_mx_control *c=(void *)(b+sizeof(*h));
    return n==10 && h->index==id && h->count==0 && h->ccount==4 && c->code==code;
}
static void handler(int sig){(void)sig;}
static int framing(void)
{
    int g,id,e;CHECK(pair(&g,&id,&e)==0);uint8_t b[256];
    CHECK(write(e,"abc",3)==3);
    CHECK(read(g,b+1,12)==-1 && errno==ENXIO);
    CHECK(read(g,(void *)1,12)==-1 && errno==EFAULT);
    CHECK(read(g,b,11)==0 && read(g,b,12)==10);
    struct nx_mx_read *h=(void *)b;
    CHECK(h->index==id && h->count==3 && !h->ccount && !memcmp(b+6,"abc",3) && b[9]==0);
    CHECK(send(g,id,"xyz",3)==16 && read(e,b,sizeof(b))==3 && !memcmp(b,"xyz",3));
    CHECK(send(g,id,"ab",2)==16 && eot(g,id)==16);
    CHECK(read(e,b,sizeof(b))==2 && !memcmp(b,"ab",2) && read(e,b,sizeof(b))==0);
    CHECK(send(g,id,"z",1)==16 && read(e,b,sizeof(b))==1 && b[0]=='z');
    CHECK(write(e,0,0)==0 && event(g,NX_M_EOT,id));
    struct nx_mx_write bad[2]={{.index=id,.count=3,.data="BAD"},{.index=id,.count=1,.data=(void *)1}};
    CHECK(write(g,bad,sizeof(bad))==-1 && errno==EFAULT);
    CHECK(send(g,id,"ok",2)==16 && read(e,b,sizeof(b))==2 && !memcmp(b,"ok",2));
    CHECK(write(g,bad,15)==-1 && errno==EINVAL);
    static const struct nx_mx_write readonly={.index=0xfff0,.count=1,.data="r"};
    CHECK(write(g,&readonly,sizeof(readonly))==-1 && errno==EFAULT);
    bad[0]=(struct nx_mx_write){.index=id,.count=1,.reserved=1,.data="r"};
    CHECK(write(g,bad,sizeof(bad[0]))==-1 && errno==EINVAL);
    CHECK(send(g,0xffff,"r",1)==-1 && errno==ENXIO);
    struct nx_mx_control unknown={NX_M_IOCTL,0};bad[0]=(struct nx_mx_write){.index=id,.ccount=4,.data=&unknown};
    CHECK(write(g,bad,sizeof(bad[0]))==-1 && errno==ENXIO);
    int d=dup(e);CHECK(d>=0 && close(e)==0 && write(d,"end",3)==3 && close(d)==0);
    CHECK(event(g,NX_M_CLOSE,id) && read(g,b,12)==10 && !memcmp(b+6,"end",3));
    CHECK(mx(NX_MX_DETACH,id,g)==0 && close(g)==0);
    return 0;
}
static int streams(int direction,int cancel)
{
    int g,id,e;CHECK(pair(&g,&id,&e)==0);char payload[401];
    for(unsigned i=0;i<sizeof(payload);++i)payload[i]='a'+i%26;
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        signal(SIGALRM,handler);
        if(direction){
            struct nx_mx_write h={.index=id,.count=sizeof(payload),.data=payload};
            int result=write(g,&h,sizeof(h));
            if(cancel)CHECK(result==15 && h.ccount==-1 && h.count==201 && h.data==payload+200);
            else CHECK(result==16);
        }else CHECK(write(e,payload,sizeof(payload))==sizeof(payload));
        _exit(0);
    }
    CHECK(sleeping(pid));
    if(cancel)CHECK(kill(pid,SIGALRM)==0 && finished(pid));
    size_t want=cancel?200:sizeof(payload),done=0;uint8_t b[256];
    while(done<want){
        ssize_t n=read(direction?e:g,b,sizeof(b));CHECK(n>0);
        if(direction){CHECK(done+n<=want && !memcmp(b,payload+done,n));done+=n;}
        else{
            size_t offset=0;
            while(offset<(size_t)n){
                struct nx_mx_read h;memcpy(&h,b+offset,sizeof(h));
                CHECK(h.index==id && !h.ccount && h.count && done+h.count<=want);
                CHECK(!memcmp(b+offset+sizeof(h),payload+done,h.count));done+=h.count;
                offset+=(sizeof(h)+h.count+1)&~1UL;
            }
        }
    }
    if(!cancel)CHECK(finished(pid));
    CHECK(close(e)==0 && close(g)==0);return 0;
}
static int flow(void)
{
    int g,id,e;CHECK(pair(&g,&id,&e)==0 && ioctl(g,NX_MXNBLK,0)==0);
    char payload[250],b[256];memset(payload,'Q',sizeof(payload));
    struct nx_mx_write h={.index=id,.count=250,.data=payload};
    CHECK(write(g,&h,sizeof(h))==15 && h.ccount==-1 && h.count==50 && h.data==payload+200);
    CHECK(read(e,b,180)==180);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){CHECK(event(g,NX_M_UBLK,id));_exit(0);}
    CHECK(sleeping(pid) && read(e,b,1)==1 && finished(pid));
    CHECK(read(e,b,sizeof(b))==19);h.ccount=0;
    CHECK(write(g,&h,sizeof(h))==16 && read(e,b,sizeof(b))==50);
    CHECK(eot(g,id)==16 && eot(g,id)==16 && event(g,NX_M_BLK,id));
    CHECK(read(e,b,sizeof(b))==0);
    for(unsigned i=0;i<NX_MX_EVENTS;++i)CHECK(write(e,0,0)==0);
    pid=fork();CHECK(pid>=0);
    if(!pid){CHECK(write(e,0,0)==0);_exit(0);}
    CHECK(sleeping(pid) && event(g,NX_M_EOT,id) && finished(pid));
    CHECK(read(g,b,sizeof(b))==NX_MX_EVENTS*10);
    CHECK(close(e)==0 && close(g)==0);return 0;
}
static int interrupts(void)
{
    int g,id,e;CHECK(pair(&g,&id,&e)==0);
    for(int group=0;group<2;++group){
        pid_t pid=fork();CHECK(pid>=0);
        if(!pid){signal(SIGALRM,handler);char b[12];CHECK(read(group?g:e,b,sizeof(b))==-1 && errno==EINTR);_exit(0);}
        CHECK(sleeping(pid) && kill(pid,SIGALRM)==0 && finished(pid));
    }
    CHECK(send(g,id,"ab",2)==16 && close(g)==0);char b[12];
    CHECK(read(e,b,sizeof(b))==2 && read(e,b,sizeof(b))==-1 && errno==ENXIO);
    void (*old)(int)=signal(SIGPIPE,SIG_IGN);
    CHECK(old!=SIG_ERR && write(e,"x",1)==-1 && errno==EPIPE);
    CHECK(signal(SIGPIPE,old)!=SIG_ERR && close(e)==0);return 0;
}
static int detach_waiter(void)
{
    int g,id,e;CHECK(pair(&g,&id,&e)==0);char payload[401];memset(payload,'O',sizeof(payload));
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        CHECK(close(e)==0);
        struct nx_mx_write h={.index=id,.count=sizeof(payload),.data=payload};
        CHECK(write(g,&h,sizeof(h))==15 && h.ccount==-1 && h.count==201);_exit(0);
    }
    CHECK(sleeping(pid) && mx(NX_MX_DETACH,id,g)==0 && close(e)==0);
    int fresh=mx(NX_MX_CHAN,0,g);CHECK(fresh==id);e=mx(NX_MX_EXTR,fresh,g);CHECK(e>=0);
    CHECK(write(e,"NEW",3)==3);uint8_t b[12];CHECK(read(g,b,sizeof(b))==10 && !memcmp(b+6,"NEW",3));
    CHECK(finished(pid) && close(e)==0 && close(g)==0);return 0;
}
static int opens(int listener,int reject,int interrupt)
{
    int g=named("/tmp/mx-data");CHECK(g>=0 && chmod("/tmp/mx-data",0666)==0);
    if(listener)CHECK(ioctl(g,NX_MXLSTN,0)==0);
    CHECK(link("/tmp/mx-data","/tmp/mx-alias")==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        CHECK(close(g)==0);signal(SIGALRM,handler);
        if(!listener && !reject && !interrupt)CHECK(setuid(123)==0);
        int fd=open("/tmp/mx-alias",O_RDWR);
        if(reject || interrupt){CHECK(fd==-1 && errno==(interrupt?EINTR:ENXIO));_exit(0);}
        CHECK(fd>=0 && write(fd,"hi",2)==2);char b[8];
        CHECK(read(fd,b,sizeof(b))==2 && !memcmp(b,"ok",2) && close(fd)==0);_exit(0);
    }
    CHECK(sleeping(pid));uint8_t b[128];
    if(listener)CHECK(read(g,b,12)==0); /* Pending long control remains queued. */
    ssize_t n=read(g,b,sizeof(b));CHECK(n>=10);
    struct nx_mx_read h;struct nx_mx_control ev;memcpy(&h,b,sizeof(h));memcpy(&ev,b+6,sizeof(ev));
    CHECK(h.index==0xfff0 && !h.count && ev.code==(listener?NX_M_OPEN:NX_M_WATCH) && ev.value==(!listener && !reject && !interrupt?123:getuid()));
    CHECK(h.ccount==(listener?4+sizeof("/tmp/mx-alias"):4));
    if(listener)CHECK(!strcmp((char *)b+10,"/tmp/mx-alias"));
    if(interrupt)CHECK(kill(pid,SIGALRM)==0);
    else if(reject==2){CHECK(close(g)==0);g=-1;}
    else if(reject)CHECK(mx(NX_MX_DETACH,h.index,g)==0);
    else{
        CHECK(mx(NX_MX_ATTACH,h.index,g)==h.index && read(g,b,sizeof(b))==8 && !memcmp(b+6,"hi",2));
        CHECK(send(g,h.index,"ok",2)==16);
    }
    CHECK(finished(pid));
    if(!reject && !interrupt)CHECK(event(g,NX_M_CLOSE,h.index) && mx(NX_MX_DETACH,h.index,g)==0);
    if(g>=0)CHECK(mx(NX_MX_CHAN,0,g)==0xfff0 && close(g)==0);
    CHECK(unlink("/tmp/mx-data")==0 && unlink("/tmp/mx-alias")==0);return 0;
}
static int end_of_file(void)
{
    int g,id,e;CHECK(pair(&g,&id,&e)==0 && eot(g,id)==16);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){CHECK(eot(g,id)==16);_exit(0);}
    char b[12];CHECK(sleeping(pid) && read(e,b,sizeof(b))==0 && finished(pid));
    CHECK(read(e,b,sizeof(b))==0 && close(e)==0 && close(g)==0);return 0;
}
static int identity(void)
{
    int old=named("/tmp/mx-old");CHECK(old>=0 && close(old)==0);
    int live=named("/tmp/mx-live");CHECK(live>=0);
    CHECK(open("/tmp/mx-old",O_RDWR)==-1 && errno==ENXIO);
    int other=mx(NX_MX_MPXN,0,0);CHECK(other>=0 && ioctl(live,NX_MXLSTN,0)==0);
    CHECK(ioctl(other,NX_MXLSTN,0)==-1 && errno==ENXIO);
    CHECK(close(live)==0 && ioctl(other,NX_MXLSTN,0)==0 && close(other)==0);
    CHECK(unlink("/tmp/mx-old")==0 && unlink("/tmp/mx-live")==0);return 0;
}
static int nested(void)
{
    int a=mx(NX_MX_MPXN,0,0),b=mx(NX_MX_MPXN,0,0);CHECK(a>=0 && b>=0);
    int first=mx(NX_MX_CHAN,0,a);CHECK(first==0xfff0 && mx(NX_MX_JOIN,b,a)==1);
    int deep=mx(NX_MX_CHAN,0,b);CHECK(deep==0xff01);
    int x=mx(NX_MX_EXTR,first,a),y=mx(NX_MX_EXTR,deep,a);CHECK(x>=0 && y>=0);
    CHECK(write(x,"AAAAAA",6)==6 && write(y,"BBBBBB",6)==6);uint8_t bytes[12];
    CHECK(read(a,bytes,sizeof(bytes))==12 && ((struct nx_mx_read *)bytes)->index==first);
    CHECK(read(a,bytes,sizeof(bytes))==12 && ((struct nx_mx_read *)bytes)->index==deep);
    CHECK(send(a,deep,"Y",1)==16 && read(y,bytes,sizeof(bytes))==1 && bytes[0]=='Y');
    CHECK(close(x)==0 && close(y)==0 && close(b)==0 && close(a)==0);return 0;
}
int mpx_data_test(void)
{
    ssize_t files=nx_kinfo(1,NX_KINFO_FILE,0,0),inodes=nx_kinfo(1,NX_KINFO_INODE,0,0);
    int error=framing();if(error)return error;
    error=streams(0,0);if(error)return error;
    error=streams(1,0);if(error)return error;
    error=streams(1,1);if(error)return error;
    error=flow();if(error)return error;
    error=end_of_file();if(error)return error;
    error=identity();if(error)return error;
    error=interrupts();if(error)return error;
    error=detach_waiter();if(error)return error;
    error=opens(0,0,0);if(error)return error;
    error=opens(1,0,0);if(error)return error;
    error=opens(0,1,0);if(error)return error;
    error=opens(0,2,0);if(error)return error;
    error=opens(0,0,1);if(error)return error;
    error=nested();if(error)return error;
    CHECK(nx_kinfo(1,NX_KINFO_FILE,0,0)==files && nx_kinfo(1,NX_KINFO_INODE,0,0)==inodes);
    const char marker[]="NEXIX_MPX_DATA_OK\n";CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);return 0;
}
