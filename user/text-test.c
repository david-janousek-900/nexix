#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static int ready_fd,go_fd;
static pid_t start(const char *path,const char *mode)
{
    int ready[2],go[2];if(pipe(ready)||pipe(go))return -1;
    char r[12],g[12];snprintf(r,sizeof(r),"%d",ready[1]);snprintf(g,sizeof(g),"%d",go[0]);
    pid_t child=fork();if(child<0)return -1;
    if(!child){close(ready[0]);close(go[1]);execl(path,"text",mode,r,g,(char *)NULL);_exit(99);}
    close(ready[1]);close(go[0]);ready_fd=ready[0];go_fd=go[1];return child;
}
static int receive(char expected)
{char c;return read(ready_fd,&c,1)==1&&c==expected;}
static int send(void){return write(go_fd,"G",1)==1;}
static int finish(pid_t child)
{int status;return send()&&close(ready_fd)==0&&close(go_fd)==0&&wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0;}
static int busy(const char *path)
{
    struct stat before,after;if(stat(path,&before))return 0;
    if(open(path,O_WRONLY)!=-1||errno!=ETXTBSY)return 0;
    if(open(path,O_RDWR|O_TRUNC)!=-1||errno!=ETXTBSY)return 0;
    if(creat(path,0600)!=-1||errno!=ETXTBSY)return 0;
    if(access(path,W_OK)!=-1||errno!=ETXTBSY)return 0;
    if(stat(path,&after)||before.st_size!=after.st_size||before.st_mtime!=after.st_mtime)return 0;
    return 1;
}
static int open_exec_busy(int mode)
{
    int fd=open("/tmp/text",mode);CHECK(fd>=0);
    CHECK(fcntl(fd,F_SETFD,FD_CLOEXEC)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){execv("/tmp/text",(char *const[]){"text",NULL});_exit(errno==ETXTBSY?0:1);}
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(fcntl(fd,F_GETFD)==FD_CLOEXEC&&close(fd)==0);return 0;
}
int text_test(void)
{
    long gate=0x904;__asm__ volatile("int $0x81":"+a"(gate)::"memory","cc");CHECK(gate==0);
    char bytes[4096];int input=open("/tmp/text",O_RDONLY),output=creat("/tmp/text2",0755);
    CHECK(input>=0&&output>=0);ssize_t n;
    while((n=read(input,bytes,sizeof(bytes)))>0)CHECK(write(output,bytes,n)==n);
    CHECK(n==0&&close(input)==0&&close(output)==0);
    int error=open_exec_busy(O_RDONLY);if(error)return error;
    error=open_exec_busy(O_WRONLY);if(error)return error;
    pid_t child=start("/tmp/text","hold");CHECK(child>=0&&receive('R')&&busy("/tmp/text"));
    pid_t denied=fork();CHECK(denied>=0);
    if(!denied){if(setuid(123))_exit(1);_exit(open("/tmp/text",O_WRONLY)==-1&&errno==ETXTBSY?0:2);}
    int denied_status;CHECK(wait(&denied_status)==denied&&WIFEXITED(denied_status)&&WEXITSTATUS(denied_status)==0);
    CHECK(chmod("/tmp/text",01755)==0&&chmod("/tmp/text",0755)==0&&busy("/tmp/text"));
    CHECK(unlink("/tmp/text")==-1&&errno==ETXTBSY);
    CHECK(link("/tmp/text","/tmp/alias")==0&&unlink("/tmp/text")==0);
    CHECK(busy("/tmp/alias")&&unlink("/tmp/alias")==-1&&errno==ETXTBSY);
    CHECK(finish(child)&&link("/tmp/alias","/tmp/text")==0&&unlink("/tmp/alias")==0);
    int fd=open("/tmp/text",O_WRONLY);CHECK(fd>=0&&close(fd)==0);
    child=start("/tmp/text","hold");CHECK(child>=0&&receive('R'));
    int saved_ready=ready_fd,saved_go=go_fd;
    fd=open("/tmp/text",O_RDONLY);CHECK(fd>=0);
    pid_t shared=start("/tmp/text","shared");CHECK(shared>=0&&receive('R')&&busy("/tmp/text")&&finish(shared)&&close(fd)==0);
    ready_fd=saved_ready;go_fd=saved_go;CHECK(busy("/tmp/text")&&finish(child));
    child=start("/tmp/text","same");CHECK(child>=0&&receive('R')&&send()&&receive('R')&&busy("/tmp/text")&&finish(child));
    child=start("/tmp/text","fork");CHECK(child>=0&&receive('F')&&busy("/tmp/text"));
    CHECK(send()&&receive('P')&&busy("/tmp/text")&&finish(child));
    child=start("/tmp/text","replace");CHECK(child>=0&&receive('R')&&busy("/tmp/text"));
    CHECK(send()&&receive('B')&&busy("/tmp/text"));
    CHECK(send()&&receive('R')&&busy("/tmp/text2"));
    fd=open("/tmp/text",O_WRONLY);CHECK(fd>=0&&close(fd)==0&&finish(child));
    /* Inactive sticky ownership admits another exec despite an open reader. */
    CHECK(chmod("/tmp/text",01755)==0);
    child=start("/tmp/text","hold");CHECK(child>=0&&receive('R')&&finish(child));
    fd=open("/tmp/text",O_RDONLY);CHECK(fd>=0);
    child=start("/tmp/text","hold");CHECK(child>=0&&receive('R')&&busy("/tmp/text")&&finish(child)&&close(fd)==0);
    CHECK(chmod("/tmp/text",0755)==0);error=open_exec_busy(O_RDONLY);if(error)return error;
    CHECK(chmod("/tmp/text",01755)==0);
    child=start("/tmp/text","hold");CHECK(child>=0&&receive('R')&&finish(child));
    fd=open("/tmp/text",O_WRONLY);CHECK(fd>=0&&close(fd)==0); /* Evicts unused sticky entry. */
    CHECK(chmod("/tmp/text",0755)==0&&unlink("/tmp/text")==0&&unlink("/tmp/text2")==0);
    CHECK(write(1,"NEXIX_TEXT_OK\n",14)==14);return 0;
}
