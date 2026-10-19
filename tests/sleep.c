/* Inject an alarm after userspace setup but before the atomic wait checks it. */
#include <unistd.h>
#include <signal.h>
#include <sys/times.h>
#include <nexix/libc.h>
#include <nexix/syscall.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
int errno;
static clock_t tick;
static unsigned remaining,calls;
static int inject;
static sighandler_t disposition;
static void original(int n){(void)n;assert(0);}
clock_t times(struct tms *t){*t=(struct tms){0};return tick;}
unsigned alarm(unsigned n){unsigned old=remaining;remaining=n;++calls;return old;}
sighandler_t signal(int n,sighandler_t handler){assert(n==SIGALRM);sighandler_t old=disposition;disposition=handler;++calls;return old;}
long nx_syscall6(long n,long deadline,long flag,long a,long b,long c,long d)
{
    assert(n==NX_SYS_TIMEDPAUSE && flag && !a && !b && !c && !d);++calls;
    if(inject){inject=0;sighandler_t handler=disposition;disposition=SIG_DFL;handler(SIGALRM);}
    if(!*(volatile sig_atomic_t *)(uintptr_t)flag)tick=deadline;
    return 0;
}
int main(void)
{
    disposition=original;remaining=5;assert(!sleep(0) && !calls && remaining==5);
    tick=0;remaining=0;assert(!sleep(2) && tick==200 && disposition==original && !remaining);
    tick=0;remaining=5;assert(!sleep(2) && tick==200 && remaining==3 && disposition==original);
    tick=0;remaining=1;assert(sleep(3)==2 && tick==100 && remaining==1 && disposition==original);
    tick=0;remaining=2;assert(!sleep(2) && tick==200 && remaining==1 && disposition==original);
    tick=0;remaining=0;inject=1;assert(sleep(3)==3 && !tick && disposition==original && !remaining);
    puts("PASS: sleep zero/no-alarm, earlier/equal/later alarms, handler restoration and alarm-before-wait race");
}
