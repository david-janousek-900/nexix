#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static long process_uid(void)
{long n=0x901;__asm__ volatile("int $0x81":"+a"(n)::"memory","cc");return n;}
static int ids(char **a)
{return getuid()==(uid_t)atoi(a[1])&&geteuid()==(uid_t)atoi(a[2])&&getgid()==(gid_t)atoi(a[3])&&getegid()==(gid_t)atoi(a[4])&&process_uid()==geteuid();}
int main(int argc,char **argv)
{
    CHECK(argc==6&&ids(argv));
    if(!strcmp(argv[5],"plain")){
        argv[5]="normal";execv("/tmp/plainid",argv);return 90;
    }
    if(!strcmp(argv[5],"rootchain")){
        argv[5]="normal";execv("/tmp/setid",argv);return 91;
    }
    int fd=open("/tmp/uid-only",O_RDONLY);
    if(geteuid()==0||geteuid()==789)CHECK(fd>=0&&close(fd)==0);
    else CHECK(fd==-1&&errno==EACCES);
    fd=open("/tmp/gid-only",O_RDONLY);
    if(geteuid()==0||getegid()==321)CHECK(fd>=0&&close(fd)==0);
    else CHECK(fd==-1&&errno==EACCES);
    if(getuid()!=0)CHECK(access("/tmp/uid-only",R_OK)==-1&&errno==EACCES);
    pid_t child=fork();CHECK(child>=0);
    if(!child)_exit(ids(argv)?0:1);
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    if(geteuid()!=0){
        if(geteuid()!=getuid())CHECK(setuid(geteuid())==-1&&errno==EPERM);
        if(getegid()!=getgid())CHECK(setgid(getegid())==-1&&errno==EPERM);
    }
    uid_t real=getuid();gid_t group=getgid();
    CHECK(setgid(group)==0&&setuid(real)==0);
    CHECK(geteuid()==real&&getuid()==real&&getegid()==group&&getgid()==group&&process_uid()==real);
    return 0;
}
