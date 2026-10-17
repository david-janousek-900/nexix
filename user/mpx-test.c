#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/kinfo.h>
#include <errno.h>
#include <stdio.h>
#include <nexix/libc.h>
#include <nexix/mpx.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"mpx line %d errno %d\n",__LINE__,errno);return __LINE__;}}while(0)
static int mx(int cmd,int a,int b,int c)
{
    struct nx_mx_args v={.arg={a,b,c}};
    long r=nx_syscall6(NX_SYS_MPX,cmd,(long)&v,0,0,0,0);
    if(r<0){errno=-r;return -1;}return r;
}
static int named(const char *path,int mode)
{
    struct nx_mx_args v={.name=path,.arg={mode,0,0}};
    long r=nx_syscall6(NX_SYS_MPX,NX_MX_MPX,(long)&v,0,0,0,0);
    if(r<0){errno=-r;return -1;}return r;
}
static int group(void){return mx(NX_MX_MPXN,0,0,0);}
static int finished(pid_t pid){int status;return wait(&status)==pid && WIFEXITED(status) && !WEXITSTATUS(status);}
static int records(int kind)
{
    ssize_t bytes=nx_kinfo(1,kind,0,0);if(bytes<0)return -1;
    return bytes;
}
static int ownership(void)
{
    int g=group();CHECK(g>=0);int id=mx(NX_MX_CHAN,0,g,0);CHECK(id==0xfff0);
    CHECK(mx(NX_MX_ATTACH,id,g,0)==id);
    int e=mx(NX_MX_EXTR,id,g,1);CHECK(e>=0);
    int d=mx(NX_MX_EXTR,id,g,0);CHECK(d>=0 && d!=e);
    CHECK(lseek(e,0,SEEK_SET)==-1 && errno==ESPIPE);
    CHECK(read(e,0,0)==0 && write(e,"x",1)==1);
    CHECK(mx(NX_MX_DETACH,id,g,0)==0);
    CHECK(mx(NX_MX_EXTR,id,g,0)==-1 && errno==ENXIO);
    int next=mx(NX_MX_CHAN,0,g,0);CHECK(next==0xfff1);
    CHECK(close(e)==0 && close(d)==0);
    CHECK(mx(NX_MX_CHAN,0,g,0)==id);
    CHECK(close(g)==0); /* Both unextracted channels are reclaimed. */
    g=group();CHECK(g>=0);id=mx(NX_MX_CHAN,0,g,0);e=mx(NX_MX_EXTR,id,g,0);CHECK(e>=0);
    CHECK(close(g)==0); /* Endpoint remains valid after parent destruction. */
    struct stat st;CHECK(fstat(e,&st)==0 && st.st_nlink==0);
    CHECK(close(e)==0);
    g=group();CHECK(g>=0);id=mx(NX_MX_CHAN,0,g,0);e=mx(NX_MX_EXTR,id,g,0);CHECK(e>=0);
    CHECK(close(e)==0 && mx(NX_MX_EXTR,id,g,0)==-1 && errno==ENXIO);
    CHECK(mx(NX_MX_DETACH,id,g,0)==0 && mx(NX_MX_CHAN,0,g,0)==id && close(g)==0);
    return 0;
}
static int capacity(void)
{
    int a=group(),b=group();CHECK(a>=0 && b>=0);
    int ids[20];
    for(int i=0;i<15;++i){ids[i]=mx(NX_MX_CHAN,0,a,0);CHECK(ids[i]==(0xfff0|i));}
    CHECK(mx(NX_MX_CHAN,0,a,0)==-1 && errno==ENXIO);
    for(int i=15;i<20;++i){ids[i]=mx(NX_MX_CHAN,0,b,0);CHECK(ids[i]>=0);}
    CHECK(mx(NX_MX_CHAN,0,b,0)==-1 && errno==ENXIO);
    CHECK(mx(NX_MX_DETACH,ids[7],a,0)==0 && mx(NX_MX_CHAN,0,b,0)==0xfff5);
    CHECK(close(a)==0 && close(b)==0);
    int groups[10];
    for(int repeat=0;repeat<3;++repeat){
        for(int i=0;i<10;++i){groups[i]=group();CHECK(groups[i]>=0);}
        CHECK(group()==-1 && errno==ENXIO);
        for(int i=0;i<10;++i)CHECK(close(groups[i])==0);
    }
    a=group();CHECK(a>=0);int fds[20],n=0;
    while(n<20 && (fds[n]=dup(a))>=0)++n;
    CHECK(n>0 && n<20 && errno==EMFILE && group()==-1 && errno==EMFILE);
    while(n)CHECK(close(fds[--n])==0);
    CHECK(close(a)==0);return 0;
}
static int trees(void)
{
    int g[5];for(int i=0;i<5;++i){g[i]=group();CHECK(g[i]>=0);}
    CHECK(mx(NX_MX_JOIN,g[0],g[0],0)==-1 && errno==EINVAL);
    for(int i=0;i<3;++i)CHECK(mx(NX_MX_JOIN,g[i+1],g[i],0)==0);
    CHECK(mx(NX_MX_JOIN,g[4],g[3],0)==-1 && errno==EINVAL);
    CHECK(mx(NX_MX_JOIN,g[0],g[3],0)==-1 && errno==EINVAL);
    CHECK(mx(NX_MX_JOIN,g[1],g[4],0)==-1 && errno==ENXIO);
    CHECK(mx(NX_MX_CHAN,0,g[3],0)==0x0000);
    int endpoint=mx(NX_MX_EXTR,0x0000,g[0],1);
    CHECK(endpoint>=0 && close(endpoint)==0);
    CHECK(mx(NX_MX_CHAN,0,g[3],0)==0x1000);
    endpoint=mx(NX_MX_EXTR,0x1000,g[0],1);CHECK(endpoint>=0);
    for(int i=1;i<4;++i)CHECK(close(g[i])==0);
    int sub=mx(NX_MX_EXTR,0xfff0,g[0],0);CHECK(sub>=0);
    CHECK(close(sub)==0 && mx(NX_MX_DETACH,0xfff0,g[0],0)==0);
    CHECK(fstat(endpoint,&(struct stat){0})==0 && close(endpoint)==0);
    CHECK(close(g[0])==0 && close(g[4])==0);
    for(int i=0;i<4;++i){g[i]=group();CHECK(g[i]>=0);}
    CHECK(mx(NX_MX_CHAN,0,g[0],0)==0xfff0);
    CHECK(mx(NX_MX_JOIN,g[1],g[0],0)==1);
    CHECK(mx(NX_MX_CHAN,0,g[1],0)==0xff01);
    CHECK(mx(NX_MX_CHAN,0,g[1],0)==0xff11);
    CHECK(mx(NX_MX_JOIN,g[2],g[1],0)==2);
    CHECK(mx(NX_MX_CHAN,0,g[2],0)==0xf021);
    CHECK(mx(NX_MX_JOIN,g[3],g[2],0)==1);
    CHECK(mx(NX_MX_CHAN,0,g[3],0)==0x0121);
    endpoint=mx(NX_MX_EXTR,0x0121,g[0],1);CHECK(endpoint>=0 && close(endpoint)==0);
    CHECK(mx(NX_MX_EXTR,0x1121,g[0],1)==-1 && errno==ENXIO);
    sub=mx(NX_MX_EXTR,0xff21,g[0],0);CHECK(sub>=0 && close(sub)==0);
    CHECK(mx(NX_MX_ATTACH,(int16_t)0xff01,g[0],0)==0xff01);
    for(int i=0;i<4;++i)CHECK(close(g[i])==0);
    return 0;
}
static int lifetime(void)
{
    int g=group(),ready[2],go[2];CHECK(g>=0 && pipe(ready)==0 && pipe(go)==0);
    pid_t p=fork();CHECK(p>=0);
    if(!p){
        CHECK(close(ready[0])==0 && close(go[1])==0);char c;
        CHECK(write(ready[1],"R",1)==1 && read(go[0],&c,1)==1);
        CHECK(mx(NX_MX_CHAN,0,g,0)==0xfff0 && close(g)==0);_exit(0);
    }
    CHECK(close(ready[1])==0 && close(go[0])==0);char c;
    CHECK(read(ready[0],&c,1)==1 && close(g)==0 && write(go[1],"G",1)==1);
    CHECK(close(ready[0])==0 && close(go[1])==0 && finished(p));
    long gate=0x911;__asm__ volatile("int $0x81":"+a"(gate)::"memory","cc");CHECK(gate==0);
    g=group();CHECK(g>=0);p=fork();CHECK(p>=0);
    if(!p){
        int closed=group();CHECK(closed>=0 && fcntl(closed,F_SETFD,FD_CLOEXEC)==0);
        char live[12],gone[12];snprintf(live,sizeof(live),"%d",g);snprintf(gone,sizeof(gone),"%d",closed);
        CHECK(execv("/tmp/no-mpx-image",(char *const[]){"none",0})==-1 && errno==ENOENT);
        CHECK(mx(NX_MX_CHAN,0,closed,0)==0xfff0);
        execl("/tmp/trace","trace","8",live,gone,(char *)0);_exit(99);
    }
    CHECK(finished(p) && close(g)==0 && unlink("/tmp/trace")==0);return 0;
}
int mpx_test(void)
{
    int files=records(NX_KINFO_FILE),inodes=records(NX_KINFO_INODE);CHECK(files>0 && inodes>0);
    CHECK(nx_syscall6(NX_SYS_MPX,NX_MX_MPXN,1,0,0,0,0)==-EFAULT);
    CHECK(mx(99,0,0,0)==-1 && errno==EINVAL);
    CHECK(mx(NX_MX_CHAN,0,-1,0)==-1 && errno==ENXIO);
    mode_t old=umask(0027);int g=named("/tmp/mpx",0666);CHECK(g>=0);umask(old);
    struct stat st;CHECK(stat("/tmp/mpx",&st)==0 && (st.st_mode&0170000)==0030000 && (st.st_mode&0777)==0640);
    CHECK(fstat(g,&st)==0 && st.st_nlink==1 && st.st_mtime>0 && st.st_ctime>0);
    CHECK(named("/tmp/mpx",0600)==-1 && errno==EEXIST);
    CHECK(unlink("/tmp/mpx")==0 && fstat(g,&st)==0 && st.st_nlink==0 && close(g)==0);
    g=named("/tmp/mpx",0666);CHECK(g>=0 && close(g)==0);
    CHECK(stat("/tmp/mpx",&st)==0 && (st.st_mode&0170000)==0020000);
    CHECK(open("/tmp/mpx",O_RDWR)==-1 && errno==ENXIO && unlink("/tmp/mpx")==0);
    g=group();CHECK(g>=0 && fstat(g,&st)==0 && st.st_nlink==0);
    CHECK(mx(NX_MX_EXTR,0xffff,g,0)==-1 && errno==ENXIO);
    CHECK(mx(NX_MX_EXTR,0x10000,g,0)==-1 && errno==ENXIO && close(g)==0);
    int error=ownership();if(error)return error;
    error=capacity();if(error)return error;
    error=trees();if(error)return error;
    error=lifetime();if(error)return error;
    pid_t p=fork();CHECK(p>=0);
    if(!p){CHECK(setuid(123)==0);g=group();CHECK(g>=0 && close(g)==0);
        CHECK(named("/mpx-denied",0600)==-1 && errno==EACCES);_exit(0);}
    CHECK(finished(p));
    CHECK(records(NX_KINFO_FILE)==files && records(NX_KINFO_INODE)==inodes);
    static const char marker[]="NEXIX_MPX_OK\n";CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);return 0;
}
