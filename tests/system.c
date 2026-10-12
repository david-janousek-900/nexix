#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
static void interrupt_handler(int n){(void)n;}
static void quit_handler(int n){(void)n;}
static sighandler_t interrupts,quits;
static int forks,signals,waits,violation,child_mode,wait_error;
static pid_t fork_result,results[4];
static int statuses[4];
pid_t fork(void)
{
    ++forks;
    if(interrupts!=interrupt_handler||quits!=quit_handler||signals)violation=1;
    if(fork_result<0)errno=EAGAIN;
    return fork_result;
}
sighandler_t signal(int n,sighandler_t handler)
{
    ++signals;sighandler_t *where;
    if(n==SIGINT)where=&interrupts;
    else if(n==SIGQUIT)where=&quits;
    else{violation=1;errno=EINVAL;return SIG_ERR;}
    sighandler_t old=*where;*where=handler;
    /* A successful restoration must not obscure a wait error. */
    if(handler!=SIG_IGN)errno=EINVAL;
    return old;
}
pid_t wait(int *status)
{
    if(interrupts!=SIG_IGN||quits!=SIG_IGN||waits>=4){violation=1;errno=ECHILD;return -1;}
    int index=waits++;*status=statuses[index];
    if(results[index]<0)errno=wait_error;
    return results[index];
}
int execl(const char *path,const char *first,...)
{
    va_list arguments;va_start(arguments,first);
    const char *option=va_arg(arguments,const char *);
    const char *command=va_arg(arguments,const char *);
    const char *end=va_arg(arguments,const char *);va_end(arguments);
    if(strcmp(path,"/bin/sh")||strcmp(first,"sh")||strcmp(option,"-c")||strcmp(command,"fixture")||end)violation=1;
    errno=ENOENT;return -1;
}
/* Only the child-mode test reaches this. Normal exit is a host fixture tool;
 * the implementation must invoke _exit(127), never exit/stdio flushing. */
_Noreturn void _exit(int code)
{
    exit(child_mode&&code==127&&!violation&&forks==1&&!signals&&!waits?0:90);
}
static void reset(void)
{
    interrupts=interrupt_handler;quits=quit_handler;
    forks=signals=waits=violation=child_mode=0;fork_result=42;
    results[0]=42;results[1]=results[2]=results[3]=-1;
    statuses[0]=7<<8;wait_error=ECHILD;
}
int main(int argc,char **argv)
{
    (void)argv;reset();
    if(argc==2){child_mode=1;fork_result=0;system("fixture");return 91;}
    CHECK(system(NULL)==-1&&errno==EINVAL&&!forks&&!signals&&!waits);
    CHECK(system("fixture")==(7<<8)&&forks==1&&signals==4&&waits==1&&!violation);
    CHECK(interrupts==interrupt_handler&&quits==quit_handler);
    reset();results[0]=11;results[1]=12;results[2]=42;
    statuses[0]=3<<8;statuses[1]=SIGTERM;statuses[2]=127<<8;
    CHECK(system("fixture")==(127<<8)&&waits==3&&!violation);
    reset();statuses[0]=SIGTERM|0200;
    CHECK(system("")== (SIGTERM|0200)&&!violation);
    reset();fork_result=-1;
    CHECK(system("fixture")==-1&&errno==EAGAIN&&forks==1&&!signals&&!waits);
    reset();results[0]=-1;wait_error=EINTR;
    CHECK(system("fixture")==-1&&errno==EINTR&&waits==1&&signals==4&&!violation);
    CHECK(interrupts==interrupt_handler&&quits==quit_handler);
    reset();results[0]=-1;
    CHECK(system("fixture")==-1&&errno==ECHILD&&waits==1&&!violation);
    return 0;
}
