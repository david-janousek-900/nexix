/* Adapt the release's persistent setsig() to V7 signal(). No POSIX masks or
 * job-control signals are fabricated; their upstream blocks stay disabled. */
#include <signal.h>
static sighandler_t handlers[NSIG];
static void dispatch(int n)
{
    sighandler_t handler=handlers[n];
    signal(n,dispatch);
    if(handler!=SIG_DFL && handler!=SIG_IGN && handler!=SIG_ERR)handler(n);
}
sighandler_t nx_vi_signal(int n,sighandler_t handler)
{
    if(n<=0 || n>=NSIG || n==SIGKILL)return signal(n,handler);
    sighandler_t old=signal(n,handler==SIG_DFL || handler==SIG_IGN?handler:dispatch);
    if(old==SIG_ERR)return old;
    if(old==dispatch)old=handlers[n];
    handlers[n]=handler;
    return old;
}

#include <sys/stat.h>
#include <errno.h>
char *nx_vi_strerror(int n)
{return n>=0 && n<sys_nerr?sys_errlist[n]:"Unknown error";}
int nx_vi_same_directory(const char *path)
{
    struct stat home,current;
    return stat(path,&home)==0 && stat(".",&current)==0 &&
        home.st_dev==current.st_dev && home.st_ino==current.st_ino;
}
