/* V7 alarm preservation with a native atomic deadline/flag wait. */
#include <signal.h>
#include <unistd.h>
#include <sys/times.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <stdint.h>
#include <errno.h>
static volatile sig_atomic_t *active;
static void wake(int number){(void)number;if(active)*active=1;}
unsigned sleep(unsigned seconds)
{
    if(!seconds)return 0;
    struct tms ticks;
    clock_t start=times(&ticks);
    if(start<0)return seconds;
    unsigned previous=alarm(0);
    unsigned span=previous && previous<seconds?previous:seconds;
    volatile sig_atomic_t interrupted=0;
    volatile sig_atomic_t *outer=active;
    active=&interrupted;
    sighandler_t handler=signal(SIGALRM,wake);
    if(handler==SIG_ERR){active=outer;if(previous)alarm(previous);return seconds;}
    uint64_t deadline=(uint64_t)start+(uint64_t)span*NX_CLK_TCK;
    for(;;){
        long result=nx_syscall6(NX_SYS_TIMEDPAUSE,deadline,(long)(uintptr_t)&interrupted,0,0,0,0);
        if(result==-EINTR)continue; /* Run unrelated handlers, then finish the wait. */
        if(result<0)errno=(int)-result;
        break;
    }
    alarm(0);
    signal(SIGALRM,handler);
    active=outer;
    clock_t finish=times(&ticks);
    unsigned elapsed=finish>=start?(uint64_t)(finish-start)/NX_CLK_TCK:0;
    if(previous)alarm(previous>elapsed?previous-elapsed:1);
    return seconds>elapsed?seconds-elapsed:0;
}
