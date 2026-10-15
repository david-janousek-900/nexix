#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(c) do {if(!(c)){fprintf(stderr,"setid line %d errno %d\n",__LINE__,errno);return __LINE__;}}while(0)
static long gate(long n,uintptr_t argument)
{__asm__ volatile("int $0x81":"+a"(n):"D"(argument):"memory","cc");return n;}
static int collect(pid_t pid)
{int status;pid_t found=wait(&status);if(found!=pid||status)fprintf(stderr,"setid child %d status %d\n",pid,status);return found==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0;}
static int run(mode_t mode,uid_t owner,gid_t group,uid_t real,uid_t effective,gid_t egid,const char *action,int traced)
{
    CHECK(chown("/tmp/setid",owner,group)==0&&chmod("/tmp/setid",mode)==0);
    char uid[12],euid[12],gid[12];
    snprintf(uid,sizeof(uid),"%u",real);snprintf(euid,sizeof(euid),"%u",effective);snprintf(gid,sizeof(gid),"%u",egid);
    char *args[]={"setid",uid,euid,"456",gid,(char *)action,NULL};
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(setgid(456)||setuid(real))_exit(97);
        if(traced){if(signal(SIGTRAP,SIG_IGN)==SIG_ERR||ptrace(0,0,0,0))_exit(98);}
        execv("/tmp/setid",args);_exit(99);
    }
    if(traced){
        int stopped;CHECK(wait(&stopped)==child&&WIFSTOPPED(stopped)&&WSTOPSIG(stopped)==SIGTRAP);
        CHECK(ptrace(NX_PT_CONTINUE,child,(void *)1,0)==0);
    }
    if(!collect(child)){fprintf(stderr,"setid case mode %o owner %u/%u caller %u expected %u/%u\n",mode,owner,group,real,effective,egid);return __LINE__;}
    CHECK(getuid()==0&&geteuid()==0&&getgid()==0&&getegid()==0&&gate(0x901,0)==0);
    return 0;
}
static void handler(int n){(void)n;}
static int unchanged(int fd)
{
    if(getuid()!=123||geteuid()!=123||getgid()!=456||getegid()!=456||gate(0x901,0)!=123)return 0;
    if(fcntl(fd,F_GETFD)!=FD_CLOEXEC||signal(SIGINT,SIG_DFL)!=handler)return 0;
    return signal(SIGINT,handler)==SIG_DFL;
}
static int failures(void)
{
    int fd=open("/dev/null",O_RDWR);CHECK(fd>=0&&fcntl(fd,F_SETFD,FD_CLOEXEC)==0);
    CHECK(signal(SIGINT,handler)==SIG_DFL&&setgid(456)==0&&setuid(123)==0);
    volatile uintptr_t invalid=1;
    char *args[]={"setid",NULL};volatile unsigned long sentinel=0x123456789abcdef0UL;
    CHECK(execv("/tmp/denied",args)==-1&&errno==EACCES&&unchanged(fd));
    CHECK(execv("/tmp/badid",args)==-1&&errno==ENOEXEC&&unchanged(fd));
    CHECK(execv("/tmp/missing",args)==-1&&errno==ENOENT&&unchanged(fd));
    CHECK(execv((const char *)invalid,args)==-1&&errno==EFAULT&&unchanged(fd));
    CHECK(execv("/tmp/setid",(char *const *)invalid)==-1&&errno==EFAULT&&unchanged(fd));
    CHECK(execve("/tmp/setid",args,(char *const *)invalid)==-1&&errno==EFAULT&&unchanged(fd));
    char *many[66];for(int i=0;i<65;++i)many[i]="x";many[65]=NULL;
    CHECK(execv("/tmp/setid",many)==-1&&errno==E2BIG&&unchanged(fd));
    CHECK(gate(0x902,3)==0);
    int result=execv("/tmp/setid",args),error=errno;
    CHECK(gate(0x903,0)==0);
    CHECK(result==-1&&error==ENOMEM&&unchanged(fd)&&sentinel==0x123456789abcdef0UL);
    CHECK(close(fd)==0);return 0;
}
static int copy_probe(const char *path)
{
    char bytes[4096];int input=open("/tmp/setid",O_RDONLY),output=creat(path,0755);
    CHECK(input>=0&&output>=0);ssize_t n;
    while((n=read(input,bytes,sizeof(bytes)))>0)CHECK(write(output,bytes,n)==n);
    CHECK(n==0&&close(input)==0&&close(output)==0);return 0;
}
int setid_test(void)
{
    int copy_error=copy_probe("/tmp/rootid");if(copy_error)return copy_error;
    copy_error=copy_probe("/tmp/plainid");if(copy_error)return copy_error;
    int fd=creat("/tmp/uid-only",0400);CHECK(fd>=0&&close(fd)==0&&chown("/tmp/uid-only",789,0)==0);
    fd=creat("/tmp/gid-only",0040);CHECK(fd>=0&&close(fd)==0&&chown("/tmp/gid-only",999,321)==0);
    int error;
    if((error=run(0755,789,321,123,123,456,"normal",0)))return error;
    if((error=run(04755,789,321,123,789,456,"normal",0)))return error;
    if((error=run(02755,789,321,123,123,321,"normal",0)))return error;
    if((error=run(06755,789,321,123,789,321,"plain",0)))return error;
    if((error=run(06755,789,321,0,0,321,"normal",0)))return error;
    if((error=run(06755,0,321,123,0,321,"normal",0)))return error;
    if((error=run(06755,65535,65535,123,65535,65535,"normal",0)))return error;
    if((error=run(06755,789,321,123,123,456,"normal",1)))return error;
    /* Genuine root elevation, followed by an executable owned by another UID. */
    CHECK(chown("/tmp/rootid",0,321)==0&&chmod("/tmp/rootid",06755)==0);
    CHECK(chown("/tmp/setid",789,321)==0&&chmod("/tmp/setid",06755)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(setgid(456)||setuid(123))_exit(97);
        char *args[]={"setid","123","0","456","321","rootchain",NULL};
        execv("/tmp/rootid",args);_exit(99);
    }
    CHECK(collect(child));
    /* Reinject the exec trap through the real stopped-child interface. */
    child=fork();CHECK(child>=0);
    if(!child){
        if(setgid(456)||setuid(123)||ptrace(0,0,0,0))_exit(97);
        execv("/tmp/setid",(char *const[]){"setid",NULL});_exit(99);
    }
    int status;CHECK(wait(&status)==child&&WIFSTOPPED(status)&&WSTOPSIG(status)==SIGTRAP);
    CHECK(ptrace(NX_PT_CONTINUE,child,(void *)1,SIGTRAP)==0);
    CHECK(wait(&status)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGTRAP);
    fd=creat("/tmp/badid",0755);CHECK(fd>=0&&write(fd,"not ELF",7)==7&&close(fd)==0);
    CHECK(chown("/tmp/badid",789,321)==0&&chmod("/tmp/badid",06755)==0);
    fd=creat("/tmp/denied",0755);CHECK(fd>=0&&close(fd)==0);
    CHECK(chown("/tmp/denied",789,321)==0&&chmod("/tmp/denied",06644)==0);
    CHECK(execv("/tmp/denied",(char *const[]){"denied",NULL})==-1&&errno==EACCES&&geteuid()==0&&getegid()==0);
    CHECK(chmod("/tmp/denied",06700)==0);
    child=fork();CHECK(child>=0);if(!child)_exit(failures());
    CHECK(collect(child));
    CHECK(unlink("/tmp/denied")==0);
    CHECK(unlink("/tmp/setid")==0&&unlink("/tmp/rootid")==0&&unlink("/tmp/plainid")==0);
    CHECK(unlink("/tmp/badid")==0&&unlink("/tmp/uid-only")==0&&unlink("/tmp/gid-only")==0);
    CHECK(write(1,"NEXIX_SETID_OK\n",15)==15);return 0;
}
