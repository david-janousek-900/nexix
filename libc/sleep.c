/* V7's alarm/setjmp sleep mechanism, with native elapsed-time accounting. */
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/times.h>
static jmp_buf sleeping;
static void wake(int n) { (void)n; longjmp(sleeping,1); }
unsigned sleep(unsigned seconds)
{
    struct tms ticks;
    clock_t start=times(&ticks);
    unsigned previous=alarm(0);
    unsigned span=previous&&previous<seconds?previous:seconds;
    if(!seconds){alarm(previous);return 0;}
    sighandler_t handler=signal(SIGALRM,wake);
    if(handler==SIG_ERR){alarm(previous);return seconds;}
    if(!setjmp(sleeping)){alarm(span);for(;;)pause();}
    alarm(0);signal(SIGALRM,handler);
    unsigned elapsed=(times(&ticks)-start)/NX_CLK_TCK;
    if(previous)alarm(previous>elapsed?previous-elapsed:1);
    return seconds>elapsed?seconds-elapsed:0;
}
