#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#define CHECK(c) do{if(!(c)){printf("syscall-policy failure line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
static volatile sig_atomic_t signals;
static void handler(int n){(void)n;signals++;}
static long call(uint64_t number){return nx_syscall6(number,-1,-1,-1,-1,-1,-1);}
int main(void)
{
    pid_t pid=getpid(),group=getpgrp();
    CHECK(signal(SIGSYS,handler)!=SIG_ERR);
    errno=EDOM;
    CHECK(call(0)==0 && call(38)==0 && errno==EDOM);
    uint64_t invalid[]={40,45,49,50,55,57,58,62,63,NX_SYS_TIMEDPAUSE+1,UINT64_MAX,UINT64_C(0x100000002),UINT64_C(0x100000026)};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++)CHECK(call(invalid[i])==-EINVAL && errno==EDOM);
    CHECK(nx_syscall6(NX_SYS_UNAME,0,0,0,0,0,0)==-EFAULT);
    CHECK(getpid()==pid && getpgrp()==group && kill(pid,0)==0 && !signals);
    puts("SYSCALL_POLICY_API_OK");return 0;
}
