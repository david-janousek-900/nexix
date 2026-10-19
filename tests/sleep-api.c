#include <unistd.h>
#include <signal.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#define CHECK(c) do{if(!(c)){printf("sleep failure line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
static volatile sig_atomic_t alarms,interrupts;
static void handler(int n){if(n==SIGALRM)alarms++;else interrupts++;}
static clock_t now(void){struct tms t;return times(&t);}
int main(void)
{
    CHECK(signal(SIGALRM,handler)!=SIG_ERR);
    alarm(3);CHECK(sleep(0)==0);unsigned remaining=alarm(0);CHECK(remaining>=2 && remaining<=3);
    clock_t start=now();CHECK(sleep(1)==0 && now()-start>=NX_CLK_TCK && !alarms);
    alarm(3);CHECK(sleep(1)==0);remaining=alarm(0);CHECK(remaining>=1 && remaining<=2);
    CHECK(signal(SIGALRM,handler)==handler);
    alarm(1);CHECK(sleep(2)==1 && !alarms);CHECK(alarm(0)==1);
    alarm(1);CHECK(sleep(1)==0 && !alarms);CHECK(alarm(0)==1);
    CHECK(signal(SIGINT,handler)!=SIG_ERR);
    pid_t parent=getpid(),child=fork();CHECK(child>=0);
    if(!child){nx_syscall6(NX_SYS_TIMEDPAUSE,now()+10,0,0,0,0,0);_exit(kill(parent,SIGINT)!=0);}
    start=now();CHECK(sleep(1)==0 && interrupts==1 && now()-start>=NX_CLK_TCK);
    int status;CHECK(wait(&status)==child && status==0);
    CHECK(signal(SIGALRM,handler)==handler);
    child=fork();CHECK(child>=0);
    if(!child){nx_syscall6(NX_SYS_TIMEDPAUSE,now()+10,0,0,0,0,0);_exit(kill(parent,SIGALRM)!=0);}
    start=now();CHECK(sleep(2)==2 && now()-start<NX_CLK_TCK && !alarms);
    CHECK(wait(&status)==child && status==0 && signal(SIGALRM,handler)==handler);
    volatile sig_atomic_t flag=1;
    start=now();CHECK(nx_syscall6(NX_SYS_TIMEDPAUSE,start+NX_CLK_TCK,(uintptr_t)&flag,0,0,0,0)==0 && now()-start<NX_CLK_TCK);
    CHECK(nx_syscall6(NX_SYS_TIMEDPAUSE,now()+1,1,0,0,0,0)==-EFAULT);
    for(int i=0;i<100;i++){
        clock_t tick=now();CHECK(nx_syscall6(NX_SYS_TIMEDPAUSE,tick,0,0,0,0,0)==0);
        CHECK(nx_syscall6(NX_SYS_TIMEDPAUSE,tick+1,0,0,0,0,0)==0 && now()>=tick+1);
    }
    CHECK(nx_syscall6(NX_SYS_TIMEDPAUSE,UINT64_MAX,0,0,0,0,0)==-EINVAL);
    puts("SLEEP_API_OK");return 0;
}
