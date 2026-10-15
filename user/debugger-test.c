#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"debugger line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
extern void trace_step_start(void),trace_step_mov(void),trace_step_syscall(void),trace_step_nop(void);
extern void trace_step_break(void),trace_step_afterbreak(void),trace_write_fault(void),trace_data_jump(void);
extern void trace_step_fork(void),trace_step_fork_return(void);
static volatile long private_data=33;
static volatile long delivered;
static void step_handler(int n){delivered=n;}
static int stopped(pid_t pid,int n)
{int s;return wait(&s)==pid&&WIFSTOPPED(s)&&WSTOPSIG(s)==n;}
static int finished(pid_t pid,int n)
{int s;return wait(&s)==pid&&WIFEXITED(s)&&WEXITSTATUS(s)==n;}
static long reg(pid_t pid,size_t offset)
{return ptrace(NX_PT_READ_U,pid,(void *)offset,0);}
static int set(pid_t pid,size_t offset,long value)
{return ptrace(NX_PT_WRITE_U,pid,(void *)offset,value);}
static int resume(pid_t pid,void *address)
{return ptrace(NX_PT_CONTINUE,pid,address,0);}
static int step(pid_t pid,void *address)
{return ptrace(NX_PT_STEP,pid,address,0);}
static int pc(pid_t pid,void *address)
{return reg(pid,NX_PT_OFFSET(rip))==(long)(uintptr_t)address&&!(reg(pid,NX_PT_OFFSET(rflags))&0x100);}
static pid_t child_stop(void)
{
    pid_t pid=fork();if(!pid){ptrace(0,0,0,0);raise(SIGINT);_exit(99);}return pid;
}
static int stepping(void)
{
    pid_t pid=child_stop();CHECK(pid>0&&stopped(pid,SIGINT));
    long initial_pc=reg(pid,NX_PT_OFFSET(rip)),initial_flags=reg(pid,NX_PT_OFFSET(rflags));
    CHECK(ptrace(NX_PT_STEP,pid,(void *)1,NSIG)==-1&&errno==EIO);
    CHECK(step(pid,0)==-1&&errno==EIO);
    CHECK(reg(pid,NX_PT_OFFSET(rip))==initial_pc&&reg(pid,NX_PT_OFFSET(rflags))==initial_flags);
    CHECK(set(pid,NX_PT_OFFSET(r12),10)==0&&step(pid,trace_step_start)==0);
    CHECK(stopped(pid,SIGTRAP)&&pc(pid,trace_step_mov)&&reg(pid,NX_PT_OFFSET(r12))==11);
    CHECK(step(pid,(void *)1)==0&&stopped(pid,SIGTRAP)&&pc(pid,trace_step_syscall));
    CHECK(reg(pid,NX_PT_OFFSET(rax))==20);
    CHECK(step(pid,(void *)1)==0&&stopped(pid,SIGTRAP)&&pc(pid,trace_step_nop));
    CHECK(reg(pid,NX_PT_OFFSET(rax))==pid);
    CHECK(step(pid,(void *)1)==0&&stopped(pid,SIGTRAP)&&pc(pid,trace_step_break));
    /* Continuing after a step must not leave TF set; next stop is INT3. */
    CHECK(resume(pid,(void *)1)==0&&stopped(pid,SIGTRAP)&&pc(pid,trace_step_afterbreak));
    CHECK(resume(pid,(void *)1)==0&&finished(pid,78));
    /* The V7 register-write path may explicitly enable the trace flag. */
    pid=child_stop();CHECK(pid>0&&stopped(pid,SIGINT));
    CHECK(set(pid,NX_PT_OFFSET(r12),20)==0&&set(pid,NX_PT_OFFSET(rflags),0x302)==0);
    CHECK(resume(pid,trace_step_start)==0&&stopped(pid,SIGTRAP)&&pc(pid,trace_step_mov));
    CHECK(reg(pid,NX_PT_OFFSET(r12))==21&&resume(pid,trace_step_afterbreak)==0&&finished(pid,78));
    /* sendsig clears TF in the handler; sigreturn restores the pending step. */
    pid=fork();CHECK(pid>=0);
    if(!pid){delivered=0;signal(SIGTERM,step_handler);ptrace(0,0,0,0);raise(SIGINT);_exit(99);}
    CHECK(stopped(pid,SIGINT)&&set(pid,NX_PT_OFFSET(r12),30)==0);
    CHECK(ptrace(NX_PT_STEP,pid,trace_step_start,SIGTERM)==0);
    CHECK(stopped(pid,SIGTRAP)&&pc(pid,trace_step_mov)&&reg(pid,NX_PT_OFFSET(r12))==31);
    CHECK(ptrace(NX_PT_READ_D,pid,(void *)&delivered,0)==SIGTERM&&errno==0);
    CHECK(resume(pid,trace_step_afterbreak)==0&&finished(pid,78));
    /* Single-step a fault: suppression and redirection must still work. */
    pid=child_stop();CHECK(pid>0&&stopped(pid,SIGINT));
    CHECK(set(pid,NX_PT_OFFSET(rdi),0)==0&&step(pid,trace_write_fault)==0&&stopped(pid,SIGSEGV));
    CHECK(set(pid,NX_PT_OFFSET(rflags),0x202)==0&&resume(pid,trace_step_afterbreak)==0&&finished(pid,78));
    /* A debugger's step over fork belongs to the parent, not its new child. */
    pid=child_stop();CHECK(pid>0&&stopped(pid,SIGINT));
    CHECK(step(pid,trace_step_fork)==0&&stopped(pid,SIGTRAP)); /* mov $2,%eax */
    CHECK(step(pid,(void *)1)==0&&stopped(pid,SIGTRAP)&&pc(pid,trace_step_fork_return));
    CHECK(reg(pid,NX_PT_OFFSET(rax))>0&&resume(pid,(void *)1)==0&&finished(pid,78));
    pid=fork();CHECK(pid>=0);
    if(!pid){signal(SIGTRAP,SIG_IGN);__asm__ volatile("int3");_exit(81);}
    CHECK(finished(pid,81));
    pid=fork();CHECK(pid>=0);
    if(!pid){delivered=0;signal(SIGTRAP,step_handler);__asm__ volatile("int3");_exit(delivered==SIGTRAP?82:83);}
    CHECK(finished(pid,82));
    return 0;
}
static int protections(void)
{
    pid_t pid=child_stop();CHECK(pid>0&&stopped(pid,SIGINT));
    uintptr_t code=(uintptr_t)trace_step_start&~7UL;
    long original=ptrace(NX_PT_READ_I,pid,(void *)code,0);CHECK(errno==0);
    CHECK(ptrace(NX_PT_WRITE_D,pid,(void *)code,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_READ_I,pid,(void *)code,0)==original&&errno==0);
    CHECK(ptrace(NX_PT_WRITE_I,pid,(void *)code,original)==0);
    CHECK(ptrace(NX_PT_WRITE_D,pid,(void *)&private_data,-4096)==0);
    CHECK(ptrace(NX_PT_READ_D,pid,(void *)&private_data,0)==-4096&&errno==0&&private_data==33);
    CHECK(ptrace(NX_PT_WRITE_I,pid,(void *)&private_data,77)==0); /* Unified native I/D. */
    CHECK(ptrace(NX_PT_WRITE_I,pid,(void *)1,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_WRITE_D,pid,(void *)1,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_WRITE_I,pid,(void *)0,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_WRITE_D,pid,(void *)0x800000000000UL,0)==-1&&errno==EIO);
    CHECK(ptrace(NX_PT_WRITE_I,pid,(void *)0xffff800000000000UL,0)==-1&&errno==EIO);
    CHECK(set(pid,NX_PT_OFFSET(rdi),code)==0&&resume(pid,trace_write_fault)==0&&stopped(pid,SIGSEGV));
    CHECK(ptrace(NX_PT_READ_I,pid,(void *)code,0)==original&&errno==0); /* RX stayed RX. */
    CHECK(ptrace(NX_PT_KILL,pid,0,0)==0);int s;CHECK(wait(&s)==pid&&s==SIGSEGV);
    pid=child_stop();CHECK(pid>0&&stopped(pid,SIGINT));
    CHECK(ptrace(NX_PT_WRITE_D,pid,(void *)&private_data,0x9090909090909090UL)==0);
    CHECK(set(pid,NX_PT_OFFSET(rdi),(long)(uintptr_t)&private_data)==0);
    CHECK(resume(pid,trace_data_jump)==0&&stopped(pid,SIGSEGV)); /* Data stayed NX. */
    CHECK(ptrace(NX_PT_KILL,pid,0,0)==0&&wait(&s)==pid&&s==SIGSEGV);return 0;
}
static unsigned char bytes[4096];
static int checksum(uint64_t *result)
{
    int fd=open("/tmp/trace",O_RDONLY);CHECK(fd>=0);uint64_t hash=14695981039346656037UL;ssize_t n;
    while((n=read(fd,bytes,sizeof(bytes)))>0)for(ssize_t i=0;i<n;++i){hash^=bytes[i];hash*=1099511628211UL;}
    CHECK(n==0&&close(fd)==0);*result=hash;return 0;
}
static int image(int mode,uint64_t hash)
{
    CHECK(chmod("/tmp/trace",mode==2?01755:0755)==0);
    int ready[2];CHECK(pipe(ready)==0);char m[12],r[12];snprintf(m,sizeof(m),"%d",mode);snprintf(r,sizeof(r),"%d",ready[1]);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){close(ready[0]);execl("/tmp/trace","trace",m,r,"0",(char *)0);_exit(99);}
    CHECK(close(ready[1])==0);uintptr_t addresses[2];
    CHECK(read(ready[0],addresses,sizeof(addresses))==sizeof(addresses)&&close(ready[0])==0&&stopped(pid,SIGINT));
    void *code=(void *)addresses[0],*data=(void *)addresses[1];
    uint64_t original=(uint64_t)ptrace(NX_PT_READ_I,pid,code,0);CHECK(errno==0);
    CHECK((original&0xffffffffffUL)==0x00000029b8UL); /* mov $41,%eax */
    uint64_t patched=(original&~0xff00UL)|0x2a00UL;
    CHECK(ptrace(NX_PT_WRITE_D,pid,data,77)==0);
    CHECK(ptrace(NX_PT_WRITE_D,pid,code,patched)==-1&&errno==EIO);
    if(mode==1||mode==2){
        CHECK(ptrace(NX_PT_WRITE_I,pid,code,patched)==-1&&errno==EIO);
        CHECK(ptrace(NX_PT_READ_I,pid,code,0)==(long)original&&errno==0);
        CHECK(ptrace(NX_PT_WRITE_I,pid,data,88)==-1&&errno==EIO);
    }else{
        /* Install INT3, restore patched instruction, rewind and step over it. */
        CHECK(ptrace(NX_PT_WRITE_I,pid,code,(patched&~255UL)|0xcc)==0);
        CHECK(resume(pid,(void *)1)==0&&stopped(pid,SIGTRAP));
        CHECK(pc(pid,(void *)(addresses[0]+1)));
        CHECK(ptrace(NX_PT_WRITE_I,pid,code,patched)==0);
        CHECK(step(pid,code)==0&&stopped(pid,SIGTRAP)&&pc(pid,(void *)(addresses[0]+5)));
        CHECK(reg(pid,NX_PT_OFFSET(rax))==42);
    }
    CHECK(open("/tmp/trace",O_WRONLY)==-1&&errno==ETXTBSY);
    CHECK(resume(pid,(void *)1)==0&&finished(pid,0));
    uint64_t after;int error=checksum(&after);if(error)return error;CHECK(after==hash);
    return 0;
}
int debugger_test(void)
{
    int error=stepping();if(error)return error;
    error=protections();if(error)return error;
    long command=0x911;__asm__ volatile("int $0x81":"+a"(command)::"memory","cc");CHECK(command==0);
    uint64_t hash;error=checksum(&hash);if(error)return error;
    for(int mode=0;mode<3;++mode){error=image(mode,hash);if(error)return error;}
    error=image(4,hash);if(error)return error;
    /* A new exec loads the original file, without another process's patches. */
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){execl("/tmp/trace","trace","3","0","0",(char *)0);_exit(99);}
    CHECK(finished(pid,0));
    /* Exec stop is before the first user instruction; step the native crt0. */
    pid=fork();CHECK(pid>=0);
    if(!pid){ptrace(0,0,0,0);execl("/tmp/trace","trace","3","0","0",(char *)0);_exit(99);}
    CHECK(stopped(pid,SIGTRAP));long entry=reg(pid,NX_PT_OFFSET(rip));
    CHECK(step(pid,(void *)1)==0&&stopped(pid,SIGTRAP)&&reg(pid,NX_PT_OFFSET(rip))==entry+2);
    CHECK(resume(pid,(void *)1)==0&&finished(pid,0));
    CHECK(chmod("/tmp/trace",0755)==0&&unlink("/tmp/trace")==0);
    CHECK(write(1,"NEXIX_DEBUGGER_OK\n",18)==18);return 0;
}
