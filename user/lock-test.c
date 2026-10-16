#include <unistd.h>
#include <sys/kinfo.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <nexix/libc.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"lock line %d errno %d\n",__LINE__,errno);return __LINE__;}}while(0)
static int process(pid_t pid,struct nx_kinfo_process *result)
{
    struct {struct nx_kinfo_header h;struct nx_kinfo_process p[16];} s;
    if(nx_kinfo(1,NX_KINFO_PROCESS,&s,sizeof(s))<0 || s.h.record_size!=sizeof(s.p[0]))return -1;
    for(unsigned i=0;i<s.h.count;++i)if(s.p[i].pid==pid){*result=s.p[i];return 0;}
    return -1;
}
static int flags(pid_t pid)
{struct nx_kinfo_process p;return process(pid,&p)?-1:p.flags;}
static int finished(pid_t pid,int code)
{int status;return wait(&status)==pid && WIFEXITED(status) && WEXITSTATUS(status)==code;}
static int permission(int initially_locked)
{
    int ready[2],go[2];CHECK(pipe(ready)==0 && pipe(go)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        CHECK(close(ready[0])==0 && close(go[1])==0);
        CHECK(flags(getpid())>=0 && !(flags(getpid())&NX_KPROC_USERLOCK));
        if(initially_locked)CHECK(lock(1)==0);
        CHECK(setuid(123)==0);
        CHECK(lock(0)==-1 && errno==EPERM);
        CHECK(lock(1)==-1 && errno==EPERM);
        CHECK(lock(-1)==-1 && errno==EPERM);
        CHECK(write(ready[1],"R",1)==1);char c;CHECK(read(go[0],&c,1)==1);
        _exit(37);
    }
    CHECK(close(ready[1])==0 && close(go[0])==0);char c;
    CHECK(read(ready[0],&c,1)==1 && c=='R');
    struct nx_kinfo_process p;CHECK(process(pid,&p)==0 && p.euid==123);
    CHECK(!!(p.flags&NX_KPROC_USERLOCK)==initially_locked);
    CHECK(write(go[1],"G",1)==1 && close(go[1])==0 && close(ready[0])==0 && finished(pid,37));
    return 0;
}
static int exec_lifetime(void)
{
    long call=0x911;__asm__ volatile("int $0x81":"+a"(call)::"memory","cc");CHECK(call==0);
    int ready[2],go[2];CHECK(pipe(ready)==0 && pipe(go)==0);
    char r[12],g[12];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(g,sizeof(g),"%d",go[0]);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        CHECK(close(ready[0])==0 && close(go[1])==0 && lock(1)==0);
        CHECK(execv("/tmp/no-lock-image",(char *const[]){"none",NULL})==-1 && errno==ENOENT);
        CHECK(flags(getpid())&NX_KPROC_USERLOCK);
        execl("/tmp/trace","trace","6",r,g,(char *)NULL);_exit(99);
    }
    CHECK(close(ready[1])==0 && close(go[0])==0);char c;
    CHECK(read(ready[0],&c,1)==1 && c=='L');
    struct nx_kinfo_process p;CHECK(process(pid,&p)==0 && (p.flags&NX_KPROC_USERLOCK));
    CHECK(!strcmp(p.command,"trace"));
    CHECK(write(go[1],"G",1)==1 && close(go[1])==0 && close(ready[0])==0 && finished(pid,0));
    CHECK(unlink("/tmp/trace")==0);return 0;
}
int lock_test(void)
{
    int original=flags(getpid());CHECK(original>=0 && !(original&NX_KPROC_USERLOCK));
    CHECK(lock(0)==0 && flags(getpid())==original);
    const int values[]={1,-1,INT32_MIN,INT32_MAX};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i){
        CHECK(lock(values[i])==0 && flags(getpid())==(original|NX_KPROC_USERLOCK));
        CHECK(lock(0)==0 && flags(getpid())==original);
    }
    CHECK(nx_syscall6(NX_SYS_LOCK,0x100000000L,0,0,0,0,0)==0);
    CHECK(flags(getpid())==(original|NX_KPROC_USERLOCK));
    unsigned char *old=sbrk(8192);CHECK(old!=(void *)-1);
    old[0]=0x12;old[8191]=0x34;
    CHECK(sched_yield()==0 && old[0]==0x12 && old[8191]==0x34 && (flags(getpid())&NX_KPROC_USERLOCK));
    CHECK(brk((void *)1)==-1 && errno==ENOMEM && (flags(getpid())&NX_KPROC_USERLOCK));
    CHECK(sbrk(-8192)==old+8192 && (flags(getpid())&NX_KPROC_USERLOCK));
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){CHECK(!(flags(getpid())&NX_KPROC_USERLOCK) && lock(1)==0);_exit(17);}
    int found=0;
    for(unsigned i=0;i<1000 && !found;++i){
        struct nx_kinfo_process p;CHECK(process(pid,&p)==0);
        if(p.state==NX_KPROC_ZOMBIE){CHECK(!(p.flags&NX_KPROC_USERLOCK) && p.status==(17<<8));found=1;}
        else CHECK(sched_yield()==0);
    }
    CHECK(found && finished(pid,17) && (flags(getpid())&NX_KPROC_USERLOCK));
    int error=permission(0);if(error)return error;
    error=permission(1);if(error)return error;
    error=exec_lifetime();if(error)return error;
    CHECK((flags(getpid())&NX_KPROC_USERLOCK) && lock(0)==0 && lock(0)==0 && flags(getpid())==original);
    static const char marker[]="NEXIX_LOCK_OK\n";
    CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);return 0;
}
