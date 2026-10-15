#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/acct.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"accounting line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static unsigned char fill[512];
static long gate(long number,unsigned arg)
{__asm__ volatile("int $0x81":"+a"(number):"D"((uintptr_t)arg):"memory","cc");return number;}
static int empty(const char *path)
{int fd=creat(path,0600);return fd>=0&&close(fd)==0;}
static int child(int mode)
{
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        if(gate(0x90e,0))_exit(99);
        if(mode==1&&(setgid(456)||setuid(123)))_exit(98);
        if(mode==2){execl("/bin/echo","echo","NEXIX_ACCT_EXEC_OK",(char *)0);_exit(97);}
        if(mode==3){execl("/absent","absent",(char *)0);if(errno!=ENOENT)_exit(96);}
        if(mode==4)raise(SIGTERM);
        if(mode==5&&!(acct((char *)1)==-1&&errno==EBUSY))_exit(95);
        if(mode==6)while(open("/dev/null",O_RDONLY)>=0){}
        if(mode==7){if(setuid(123)||acct(NULL)!=-1||errno!=EPERM)_exit(94);}
        _exit(42);
    }
    int status;CHECK(wait(&status)==pid);
    if(mode==4)CHECK(WIFSIGNALED(status)&&WTERMSIG(status)==SIGTERM);
    else CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==(mode==2?0:42));
    return 0;
}
static int read_one(const char *path,off_t offset,struct acct *record)
{
    int fd=open(path,O_RDONLY);CHECK(fd>=0&&lseek(fd,offset,SEEK_SET)==offset);
    CHECK(read(fd,record,sizeof(*record))==sizeof(*record));CHECK(close(fd)==0);return 0;
}
int accounting_test(void)
{
    CHECK(acct(NULL)==0&&acct((char *)1)==-1&&errno==EFAULT);
    CHECK(acct("/absent")==-1&&errno==ENOENT);
    CHECK(acct("/tmp")==-1&&errno==EACCES&&acct("/dev/null")==-1&&errno==EACCES);
    CHECK(empty("/tmp/acct"));struct stat st;struct acct record;
    for(int mode=0;mode<8;++mode){
        CHECK(empty("/tmp/acct")&&chmod("/tmp/acct",0)==0&&acct("/tmp/acct")==0);
        CHECK(acct((char *)1)==-1&&errno==EBUSY);
        int error=child(mode);if(error)return error;
        CHECK(acct(NULL)==0);error=read_one("/tmp/acct",0,&record);if(error)return error;
        CHECK(stat("/tmp/acct",&st)==0&&st.st_size==40);
        CHECK(record.ac_flag==(mode==2?0:(mode==1||mode==5||mode==7?AFORK|ASU:AFORK)));
        CHECK(record.ac_uid==(mode==1||mode==7?123:0)&&record.ac_gid==(mode==1?456:0));
        CHECK(record.ac_utime>=0x2401&&record.ac_utime<=0x2402&&record.ac_stime==0x4400);
        time_t now=time(NULL);
        CHECK(record.ac_etime>=17&&record.ac_etime<=20&&record.ac_btime>=now-20&&record.ac_btime<=now-17);
        CHECK(record.ac_tty==0x100&&!record.ac_mem&&!record.ac_io);
        for(unsigned i=0;i<sizeof(record.ac_reserved);++i)CHECK(!record.ac_reserved[i]);
        if(mode==2)CHECK(!memcmp(record.ac_comm,"echo\0\0\0\0\0\0",10));
        else CHECK(!memcmp(record.ac_comm,"abcdefghij",10));
    }
    /* No user descriptor is consumed when enabling or writing accounting. */
    CHECK(empty("/tmp/acct"));int fds[20],count=0,fd;
    while((fd=open("/dev/null",O_RDONLY))>=0)fds[count++]=fd;
    CHECK(errno==EMFILE&&acct("/tmp/acct")==0);
    int error=child(0);if(error)return error;
    CHECK(acct(NULL)==0);while(count)CHECK(close(fds[--count])==0);
    CHECK(stat("/tmp/acct",&st)==0&&st.st_size==40);
    /* The held accounting inode survives unlink and the user reader's closes. */
    CHECK(empty("/tmp/acct"));fd=open("/tmp/acct",O_RDONLY);CHECK(fd>=0&&acct("/tmp/acct")==0&&unlink("/tmp/acct")==0);
    error=child(0);if(error)return error;
    CHECK(acct(NULL)==0&&read(fd,&record,40)==40&&close(fd)==0);
    CHECK(gate(0x909,0)==0&&gate(0x90a,0)==0);
    CHECK(mknod("/tmp/acdev",S_IFBLK|0600,1)==0&&mknod("/tmp/acrodev",S_IFBLK|0600,3)==0);
    CHECK(empty("/tmp/acmount")&&mount("/tmp/acdev","/tmp/acmount",0)==0);
    CHECK(acct("/tmp/acmount/data")==0&&umount("/tmp/acdev")==-1&&errno==EBUSY);
    error=child(0);if(error)return error;
    CHECK(acct(NULL)==0&&umount("/tmp/acdev")==0);
    CHECK(mount("/tmp/acdev","/tmp/acmount",0)==0);error=read_one("/tmp/acmount/data",7,&record);if(error)return error;
    CHECK(!memcmp(record.ac_comm,"abcdefghij",10)&&record.ac_flag==AFORK);
    /* Record the real IDs after successful SUID exec back to effective root. */
    CHECK(empty("/tmp/acct")&&chmod("/tmp/acmount/probe",05755)==0&&acct("/tmp/acct")==0);
    int ready[2],go[2];CHECK(pipe(ready)==0&&pipe(go)==0);char r[12],g[12];
    snprintf(r,sizeof(r),"%d",ready[1]);snprintf(g,sizeof(g),"%d",go[0]);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){close(ready[0]);close(go[1]);if(gate(0x90e,0)||setgid(456)||setuid(123))_exit(99);
        execl("/tmp/acmount/probe","text","account",r,g,(char *)0);_exit(98);}
    CHECK(close(ready[1])==0&&close(go[0])==0);char byte;
    CHECK(read(ready[0],&byte,1)==1&&byte=='R');
    struct stat before,after;CHECK(stat("/tmp/acmount/probe",&before)==0&&acct(NULL)==0&&acct("/tmp/acmount/probe")==0);
    error=child(0);if(error)return error; /* An active executable must not be changed by the accounting writer. */
    CHECK(acct(NULL)==0&&stat("/tmp/acmount/probe",&after)==0&&before.st_size==after.st_size);
    CHECK(acct("/tmp/acct")==0&&write(go[1],"G",1)==1&&close(ready[0])==0&&close(go[1])==0);
    int status;CHECK(wait(&status)==pid&&status==0&&acct(NULL)==0);
    error=read_one("/tmp/acct",0,&record);if(error)return error;
    CHECK(record.ac_uid==123&&record.ac_gid==456&&record.ac_flag==ASU&&!memcmp(record.ac_comm,"probe\0\0\0\0\0",10));
    CHECK(unlink("/tmp/acct")==0);
    CHECK(acct("/tmp/acmount/probe")==0); /* The now-unused sticky text is evicted on write. */
    error=child(0);if(error)return error;
    CHECK(acct(NULL)==0&&stat("/tmp/acmount/probe",&after)==0&&after.st_size==before.st_size+40);
    /* A failed, sector-crossing record never extends the visible EOF. */
    memset(fill,'X',sizeof(fill));fd=creat("/tmp/acmount/log",0600);CHECK(fd>=0&&write(fd,fill,500)==500&&close(fd)==0);
    fd=creat("/tmp/acmount/fill",0600);CHECK(fd>=0);ssize_t n;
    while((n=write(fd,fill,sizeof(fill)))>0){}
    CHECK(n==-1&&errno==ENOSPC&&close(fd)==0&&acct("/tmp/acmount/log")==0);
    error=child(0);if(error)return error;CHECK(acct(NULL)==0&&stat("/tmp/acmount/log",&st)==0&&st.st_size==500);
    CHECK(unlink("/tmp/acmount/fill")==0&&acct("/tmp/acmount/log")==0);
    error=child(0);if(error)return error;CHECK(acct(NULL)==0&&stat("/tmp/acmount/log",&st)==0&&st.st_size==540);
    CHECK(acct("/tmp/acmount/log")==0&&gate(0x90f,1)==0);
    error=child(0);if(error)return error; /* Exit status survives a data-write failure. */
    CHECK(gate(0x90f,0)==0&&gate(0x90d,1)==0);
    CHECK(stat("/tmp/acmount/log",&st)==0&&st.st_size==540&&acct(NULL)==0);
    CHECK(umount("/tmp/acdev")==0&&mount("/tmp/acrodev","/tmp/acmount",1)==0);
    CHECK(acct("/tmp/acmount/data")==0);
    error=child(0);if(error)return error;
    CHECK(acct(NULL)==0&&stat("/tmp/acmount/data",&st)==0&&st.st_size==7&&umount("/tmp/acrodev")==0);
    CHECK(unlink("/tmp/acdev")==0&&unlink("/tmp/acrodev")==0&&unlink("/tmp/acmount")==0);
    const char marker[]="NEXIX_ACCOUNTING_OK\n";CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);
    return 0;
}
