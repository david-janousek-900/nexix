#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <nexix/libc.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"ptrace line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static volatile long words[2]={-1,-4096};
extern int trace_register_probe(void);
extern void trace_redirect(void);
static volatile sig_atomic_t alarm_seen;
static void tick(int n){alarm_seen=n;}
static int stopped(pid_t pid,int signal_number)
{int status;return wait(&status)==pid&&WIFSTOPPED(status)&&WSTOPSIG(status)==signal_number&&!WIFSIGNALED(status)&&!WIFEXITED(status);}
static int finished(pid_t pid,int code)
{int status;return wait(&status)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==code;}
static long peek(pid_t pid,size_t offset)
{return ptrace(NX_PT_READ_U,pid,(void *)offset,0);}
static int poke(pid_t pid,size_t offset,long word)
{return ptrace(NX_PT_WRITE_U,pid,(void *)offset,word);}
static int resume(pid_t pid,int n)
{return ptrace(NX_PT_CONTINUE,pid,(void *)1,n);}
int ptrace_test(void)
{
    CHECK(ptrace(1,getpid(),0,0)==-1&&errno==ESRCH);
    CHECK(ptrace(1,30000,0,0)==-1&&errno==ESRCH);
    int ready[2],go[2];CHECK(pipe(ready)==0&&pipe(go)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        close(ready[0]);close(go[1]);
        if(ptrace(0,30000,(void *)1,-1)||write(ready[1],"R",1)!=1)_exit(98);
        char byte;if(read(go[0],&byte,1)!=1)_exit(99);
        if(signal(SIGTERM,SIG_IGN)==SIG_ERR)_exit(96);
        int code=trace_register_probe();if(code)_exit(code);
        /* Tracing is not inherited across fork. */
        pid_t nested=fork();if(nested<0)_exit(95);
        if(!nested){signal(SIGTERM,SIG_DFL);raise(SIGTERM);_exit(94);}
        int status;
        _exit(wait(&status)==nested&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGTERM?0:93);
    }
    CHECK(close(ready[1])==0&&close(go[0])==0);
    char byte;CHECK(read(ready[0],&byte,1)==1&&close(ready[0])==0);
    /* A traced but sleeping/running child is not accessible. */
    CHECK(ptrace(NX_PT_READ_D,child,(void *)words,0)==-1&&errno==ESRCH);
    CHECK(write(go[1],"G",1)==1&&close(go[1])==0);
    CHECK(stopped(child,SIGTERM)); /* Even an ignored signal stops. */
    /* A second wait must block until a new stop/report or exit. */
    CHECK(signal(SIGALRM,tick)!=SIG_ERR);alarm_seen=0;alarm(1);
    int status;CHECK(wait(&status)==-1&&errno==EINTR&&alarm_seen==SIGALRM);
    alarm(0);CHECK(signal(SIGALRM,SIG_DFL)!=SIG_ERR);
    pid_t sibling=fork();CHECK(sibling>=0);
    if(!sibling)_exit(ptrace(NX_PT_READ_D,child,(void *)words,0)==-1&&errno==ESRCH?0:1);
    CHECK(finished(sibling,0)); /* No root or sibling access exemption. */
    errno=123;CHECK(ptrace(NX_PT_READ_D,child,(void *)words,0)==-1&&errno==0);
    CHECK(ptrace(NX_PT_READ_D,child,(void *)(words+1),0)==-4096&&errno==0);
    long instruction;memcpy(&instruction,(void *)trace_register_probe,8);
    CHECK(ptrace(NX_PT_READ_I,child,(void *)trace_register_probe,0)==instruction&&errno==0);
    CHECK(ptrace(NX_PT_READ_D,child,(void *)1,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_READ_D,child,(void *)0x800000000000UL,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_READ_D,child,0,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_READ_U,child,(void *)sizeof(struct nx_ptrace_user),0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_READ_U,child,(void *)3,0)==-1&&errno==EIO);
    CHECK(nx_syscall6(NX_SYS_PTRACE,1,child,(long)(uintptr_t)words,0,1,0)==-EFAULT);
    long output=77;
    CHECK(nx_syscall6(NX_SYS_PTRACE,1,child,0,0,(long)(uintptr_t)&output,0)==-EIO&&output==77);
    CHECK(peek(child,NX_PT_OFFSET(r12))==0x12345678&&errno==0);
    CHECK(peek(child,NX_PT_OFFSET(cs))==0x23&&peek(child,NX_PT_OFFSET(ss))==0x1b);
    CHECK(peek(child,NX_PT_OFFSET(fpu)+160)==0x76543210);
    CHECK(poke(child,NX_PT_OFFSET(r12),0x23456789)==0);
    CHECK(poke(child,NX_PT_OFFSET(fpu)+160,0x3456789a)==0);
    long mxcsr=peek(child,NX_PT_OFFSET(fpu)+24);
    CHECK(poke(child,NX_PT_OFFSET(fpu)+24,-1)==-1&&errno==EIO);
    CHECK(peek(child,NX_PT_OFFSET(fpu)+24)==mxcsr);
    CHECK(poke(child,NX_PT_OFFSET(cs),8)==-1&&errno==EIO);
    CHECK(poke(child,NX_PT_OFFSET(ss),16)==-1&&errno==EIO);
    CHECK(poke(child,NX_PT_OFFSET(vector),1)==-1&&errno==EIO);
    CHECK(poke(child,NX_PT_OFFSET(error),1)==-1&&errno==EIO);
    CHECK(poke(child,NX_PT_OFFSET(rip),0)==-1&&errno==EIO);
    CHECK(poke(child,NX_PT_OFFSET(rsp),0)==-1&&errno==EIO);
    CHECK(poke(child,NX_PT_OFFSET(rflags),-1)==0);
    long flags=peek(child,NX_PT_OFFSET(rflags));
    CHECK((flags&0x3202)==0x202&&(flags&0x100)!=0);
    CHECK(poke(child,NX_PT_OFFSET(rflags),0x202)==0);
    CHECK(resume(child,NSIG)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_CONTINUE,child,0,0)==-1&&errno==EIO);
    CHECK(ptrace(4,child,0,0)==-1&&errno==EIO);
    CHECK(ptrace(5,child,0,0)==-1&&errno==EIO);
    CHECK(ptrace(9,child,0,0)==-1&&errno==EIO);
    CHECK(ptrace(999,child,0,0)==-1&&errno==EIO);
    CHECK(stopped(child,SIGTERM)); /* Examine/restop enables another report. */
    CHECK(resume(child,0)==0&&finished(child,0));
    CHECK(ptrace(NX_PT_KILL,child,0,0)==-1&&errno==ESRCH);
    /* Replacement delivery, repeated stops and continue PC redirection. */
    child=fork();CHECK(child>=0);
    if(!child){if(ptrace(0,0,0,0))_exit(99);raise(SIGINT);raise(SIGTERM);_exit(98);}
    CHECK(stopped(child,SIGINT));CHECK(resume(child,0)==0);
    CHECK(stopped(child,SIGTERM));
    CHECK(ptrace(NX_PT_CONTINUE,child,(void *)trace_redirect,0)==0&&finished(child,73));
    child=fork();CHECK(child>=0);
    if(!child){ptrace(0,0,0,0);raise(SIGINT);_exit(99);}
    CHECK(stopped(child,SIGINT));CHECK(resume(child,SIGTERM)==0);
    CHECK(wait(&status)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGTERM);
    child=fork();CHECK(child>=0);
    if(!child){ptrace(-1,0,0,0);raise(SIGINT);_exit(99);}
    CHECK(stopped(child,SIGINT));CHECK(ptrace(NX_PT_KILL,child,0,0)==0);
    CHECK(wait(&status)==child&&status==SIGINT);
    /* V7 traces SIGKILL too; a selected ignored replacement stays ignored. */
    child=fork();CHECK(child>=0);
    if(!child){ptrace(0,0,0,0);signal(SIGTERM,SIG_IGN);raise(SIGKILL);raise(SIGTERM);_exit(74);}
    CHECK(stopped(child,SIGKILL)&&resume(child,0)==0);
    CHECK(stopped(child,SIGTERM)&&resume(child,SIGTERM)==0&&finished(child,74));
    child=fork();CHECK(child>=0);
    if(!child){alarm_seen=0;signal(SIGINT,tick);ptrace(0,0,0,0);raise(SIGINT);_exit(alarm_seen==SIGINT?75:76);}
    CHECK(stopped(child,SIGINT)&&resume(child,SIGINT)==0&&finished(child,75));
    /* Synchronous faults stop before default action and can be redirected. */
    child=fork();CHECK(child>=0);
    if(!child){ptrace(0,0,0,0);__asm__ volatile("ud2");_exit(99);}
    CHECK(stopped(child,SIGILL));
    CHECK(poke(child,NX_PT_OFFSET(rip),(long)(uintptr_t)trace_redirect)==0);
    CHECK(resume(child,0)==0&&finished(child,73));
    /* Debugger exit must release a stopped orphan. Bootstrap verifies its
     * final signal status and reaps it after this user fixture completes. */
    CHECK(pipe(ready)==0&&pipe(go)==0);
    pid_t debugger=fork();CHECK(debugger>=0);
    if(!debugger){
        close(ready[0]);close(go[1]);
        pid_t subject=fork();if(subject<0)_exit(92);
        if(!subject){ptrace(0,0,0,0);raise(SIGINT);_exit(91);}
        if(!stopped(subject,SIGINT)||write(ready[1],&subject,sizeof(subject))!=sizeof(subject))_exit(90);
        char byte;_exit(read(go[0],&byte,1)==1?0:89);
    }
    CHECK(close(ready[1])==0&&close(go[0])==0);
    pid_t subject;CHECK(read(ready[0],&subject,sizeof(subject))==sizeof(subject)&&close(ready[0])==0);
    long command=0x910;
    __asm__ volatile("int $0x81":"+a"(command):"D"((uintptr_t)subject):"memory","cc");
    CHECK(command==0&&write(go[1],"G",1)==1&&close(go[1])==0&&finished(debugger,0));
    CHECK(write(1,"NEXIX_PTRACE_OK\n",16)==16);return 0;
}
