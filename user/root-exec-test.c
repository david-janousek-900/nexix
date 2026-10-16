#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(c) do { if (!(c)) {fprintf(stderr,"root-exec line %d errno %d\n",__LINE__,errno);return __LINE__;} } while (0)
static char buffer[4096];
extern int setid_test(void);
extern int text_test(void);
extern int core_test(void);
extern int storage_test(void);
extern int namespace_test(void);
extern int umount_test(void);
extern int accounting_test(void);
extern int ptrace_test(void);
extern int debugger_test(void);
extern int profile_test(void);
extern int monitor_test(void);
extern int nlist_test(void);
extern int kinfo_test(void);
static int child_exec(const char *path, int unprivileged)
{
    int pair[2]; CHECK(pipe(pair)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){close(pair[0]);dup2(pair[1],1);close(pair[1]);
        if(unprivileged && setuid(100))_exit(98);
        execl(path,"echo","disk-exec",(char *)0);_exit(99);}
    close(pair[1]);size_t count=0;ssize_t n;
    while(count<sizeof(buffer)-1 && (n=read(pair[0],buffer+count,sizeof(buffer)-1-count))>0)count+=n;
    CHECK(close(pair[0])==0);buffer[count]=0;
    int status;CHECK(wait(&status)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(!strcmp(buffer,"disk-exec\n"));return 0;
}
int main(void)
{
    CHECK(open("/dev/console",O_RDWR)==0 && dup(0)==1 && dup(0)==2);
    int null=open("/dev/null",O_RDWR);CHECK(null>=0);
    CHECK(write(null,"discard",7)==7 && read(null,buffer,sizeof(buffer))==0);
    CHECK(lseek(null,7,SEEK_SET)==7 && read(null,buffer,1)==0);
    CHECK(write(null,(void *)1,1)==-1 && errno==EFAULT);
    CHECK(read(null,(void *)1,1)==-1 && errno==EFAULT);
    struct stat null_stat;CHECK(fstat(null,&null_stat)==0 && (null_stat.st_mode&S_IFMT)==S_IFCHR && null_stat.st_size==0);
    CHECK(close(null)==0);
    char *args[]={"test",NULL};
    CHECK(execv("/bin/libc-test",args)==-1 && errno==ENOENT); /* No catalog fallback. */
    CHECK(execv("/etc/passwd",args)==-1 && errno==EACCES);
    CHECK(execv("/bin",args)==-1 && errno==EACCES);
    CHECK(execv("/dev/console",args)==-1 && errno==EACCES);
    int input=open("/bin/echo",O_RDONLY),output=open("/tmp/disk-echo",O_WRONLY|O_CREAT|O_TRUNC,0755);
    CHECK(input>=0 && output>=0);ssize_t n;
    while((n=read(input,buffer,sizeof(buffer)))>0)CHECK(write(output,buffer,n)==n);
    CHECK(n==0 && close(input)==0 && close(output)==0);
    int error=child_exec("/tmp/disk-echo",0);if(error)return error;
    CHECK(chmod("/tmp/disk-echo",0700)==0);
    pid_t restricted=fork();CHECK(restricted>=0);
    if(!restricted){
        if(setuid(100))_exit(98);
        execv("/tmp/disk-echo",args);_exit(errno==EACCES?77:99);
    }
    int status;CHECK(wait(&status)==restricted && WIFEXITED(status) && WEXITSTATUS(status)==77);
    CHECK(chmod("/tmp/disk-echo",0755)==0);
    CHECK(chdir("/tmp")==0);error=child_exec("disk-echo",1);if(error)return error;
    CHECK(chmod("disk-echo",0644)==0);
    CHECK(execv("disk-echo",args)==-1 && errno==EACCES);
    CHECK(chmod("disk-echo",04755)==0);
    error=child_exec("disk-echo",1);if(error)return error;
    CHECK(unlink("disk-echo")==0);
    output=open("bad-image",O_WRONLY|O_CREAT|O_TRUNC,0755);CHECK(output>=0);
    CHECK(write(output,"not ELF",7)==7 && close(output)==0);
    CHECK(execv("bad-image",args)==-1 && errno==ENOEXEC);
    CHECK(unlink("bad-image")==0 && chdir("/")==0);
    error=setid_test();if(error)return error;
    error=text_test();if(error)return error;
    error=core_test();if(error)return error;
    error=storage_test();if(error)return error;
    error=namespace_test();if(error)return error;
    error=umount_test();if(error)return error;
    error=accounting_test();if(error)return error;
    error=ptrace_test();if(error)return error;
    error=debugger_test();if(error)return error;
    error=profile_test();if(error)return error;
    error=monitor_test();if(error)return error;
    error=nlist_test();if(error)return error;
    error=kinfo_test();if(error)return error;
    const char marker[]="NEXIX_ROOT_EXEC_USER_OK\n";
    CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);
    return 0;
}
