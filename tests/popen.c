/* Independent failure/ordering fixtures for the V7 process-pipe adapter. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
static FILE streams[20];
static int descriptors[20],next_pipe,pipe_error,stream_error,fork_error,close_error,dup_error;
static int forks,waits,signals,violation,child_mode,reading;
static int child_source,child_target;
static pid_t next_child=100,results[8];
static int statuses[8],result_count,result_position,wait_error;
static void handler(int n){(void)n;}
static sighandler_t dispositions[NSIG];
int pipe(int fds[2])
{
    if(pipe_error){errno=EMFILE;return -1;}
    fds[0]=next_pipe;fds[1]=next_pipe+1;
    descriptors[next_pipe]=descriptors[next_pipe+1]=1;next_pipe+=2;return 0;
}
int close(int fd)
{if(fd<0||fd>=20||!descriptors[fd]){violation=1;errno=EBADF;return -1;}descriptors[fd]=0;return 0;}
FILE *fdopen(int fd,const char *mode)
{
    if(stream_error){errno=EMFILE;return NULL;}
    for(unsigned i=0;i<20;++i)if(!streams[i].used){
        streams[i]=(FILE){.used=1,.fd=fd};reading=*mode=='r';return &streams[i];
    }
    errno=EMFILE;return NULL;
}
int fclose(FILE *f)
{
    if(!f->used){violation=1;errno=EBADF;return EOF;}
    int fd=f->fd;*f=(FILE){0};close(fd);
    if(close_error){errno=EPIPE;return EOF;}return 0;
}
pid_t fork(void)
{
    ++forks;
    if(fork_error){errno=EAGAIN;return -1;}
    return child_mode?0:next_child++;
}
int dup2(int fd,int target)
{
    child_source=fd;child_target=target;
    if(!descriptors[fd])violation=1;
    if(dup_error){errno=EBADF;return -1;}
    descriptors[target]=1;return target;
}
int execl(const char *path,const char *first,...)
{
    va_list ap;va_start(ap,first);
    const char *option=va_arg(ap,const char *),*command=va_arg(ap,const char *),*end=va_arg(ap,const char *);va_end(ap);
    if(strcmp(path,"/bin/sh")||strcmp(first,"sh")||strcmp(option,"-c")||strcmp(command,"fixture")||end)violation=1;
    if(child_target!=(reading?1:0)||!descriptors[child_target])violation=1;
    for(int i=0;i<20;++i)if(descriptors[i]&&i!=child_target)violation=1;
    errno=ENOENT;return -1;
}
_Noreturn void _exit(int code)
{exit(child_mode&&code==1&&!violation&&forks==1&&!signals&&!waits?0:90);}
sighandler_t signal(int n,sighandler_t next)
{++signals;sighandler_t old=dispositions[n];dispositions[n]=next;if(next!=SIG_IGN)errno=EINVAL;return old;}
pid_t wait(int *status)
{
    ++waits;
    if(dispositions[SIGINT]!=SIG_IGN||dispositions[SIGQUIT]!=SIG_IGN||dispositions[SIGHUP]!=SIG_IGN)violation=1;
    if(result_position==result_count){errno=wait_error;return -1;}
    int index=result_position++;*status=statuses[index];return results[index];
}
static void reset(void)
{
    next_pipe=3;pipe_error=stream_error=fork_error=close_error=dup_error=0;
    forks=waits=signals=violation=child_mode=0;result_position=result_count=0;wait_error=ECHILD;
    for(int i=0;i<NSIG;++i)dispositions[i]=handler;
}
static int clean(void)
{
    if(violation)return 0;
    for(int i=0;i<20;++i)if(descriptors[i]||streams[i].used)return 0;
    return dispositions[SIGINT]==handler&&dispositions[SIGQUIT]==handler&&dispositions[SIGHUP]==handler;
}
static void result(pid_t pid,int status)
{results[result_count]=pid;statuses[result_count++]=status;}
int main(int argc,char **argv)
{
    reset();
    if(argc==2){
        child_mode=1;next_pipe=0;
        if(!strcmp(argv[1],"dup-fail"))dup_error=1;
        popen("fixture",!strcmp(argv[1],"write-child")?"w":"r");return 91;
    }
    CHECK(popen(NULL,"r")==NULL&&errno==EINVAL&&!forks&&clean());
    CHECK(popen("fixture",NULL)==NULL&&errno==EINVAL&&clean());
    CHECK(popen("fixture","rb")==NULL&&errno==EINVAL&&clean());
    CHECK(pclose(NULL)==-1&&errno==EINVAL&&clean());
    FILE ordinary={.used=1,.fd=19};CHECK(pclose(&ordinary)==-1&&ordinary.used&&errno==EINVAL);
    pipe_error=1;CHECK(popen("fixture","r")==NULL&&errno==EMFILE&&!forks&&clean());
    reset();stream_error=1;CHECK(popen("fixture","r")==NULL&&errno==EMFILE&&!forks&&clean());
    reset();fork_error=1;CHECK(popen("fixture","w")==NULL&&errno==EAGAIN&&forks==1&&clean());
    reset();pid_t pid=next_child;FILE *f=popen("fixture","r");CHECK(f&&f->fd==3&&descriptors[3]&&!descriptors[4]);
    result(99,12<<8);result(pid,7<<8);CHECK(pclose(f)==(7<<8)&&waits==2&&signals==6&&clean());
    CHECK(pclose(f)==-1&&errno==EINVAL&&waits==2);
    reset();pid=next_child;f=popen("fixture","w");CHECK(f&&f->fd==4&&!descriptors[3]);
    close_error=1;result(pid,SIGTERM|0200);CHECK(pclose(f)==(SIGTERM|0200)&&errno==EPIPE&&clean());
    reset();f=popen("fixture","r");CHECK(f);wait_error=EINTR;
    CHECK(pclose(f)==-1&&errno==EINTR&&waits==1&&clean());
    reset();f=popen("fixture","r");CHECK(f);
    CHECK(pclose(f)==-1&&errno==ECHILD&&clean());
    reset();pid_t first=next_child;FILE *a=popen("fixture","r");CHECK(a);
    pid_t second=next_child;FILE *b=popen("fixture","w");CHECK(b);
    result(first,3<<8);result(second,9<<8);
    CHECK(pclose(b)==(9<<8)&&waits==2&&pclose(a)==(3<<8)&&waits==2&&clean());
    for(int i=0;i<30;++i){reset();pid=next_child;f=popen("fixture","r");CHECK(f);result(pid,0);CHECK(pclose(f)==0&&clean());}
    return 0;
}
