#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <utmp.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
static int accounts(void)
{
    int tty=open("/dev/console",O_RDWR);CHECK(tty>=0);
    CHECK(dup2(tty,0)==0&&dup2(0,1)==1&&dup2(0,2)==2);
    if(tty>2)CHECK(close(tty)==0);
    CHECK(!strcmp(ttyname(0),"/dev/console"));
    int regular=open("/hello",O_RDONLY);CHECK(regular>=0);
    CHECK(ttyname(regular)==NULL&&errno==ENOTTY&&close(regular)==0);
    CHECK(ttyname(-1)==NULL&&errno==EBADF);
    CHECK(ttyslot()==0&&getlogin()==NULL);
    CHECK(mknod("/etc",S_IFDIR|0755,0)==0&&link("/etc","/etc/.")==0&&link("/","/etc/..")==0);
    FILE *f=fopen("/etc/ttys","w");CHECK(f);
    /* A disabled entry is still a physical slot; there is no slot-zero user. */
    CHECK(fputs("00unused\n00console\n",f)==0&&fclose(f)==0);
    CHECK(ttyslot()==2&&getlogin()==NULL&&errno==ENOENT);
    struct utmp entries[3]={0};
    memcpy(entries[0].ut_name,"WRONG",5);
    memcpy(entries[2].ut_line,"console",7);
    memcpy(entries[2].ut_name,"abcdefgh",8);entries[2].ut_time=1700000000;
    f=fopen("/etc/utmp","w");CHECK(f);
    CHECK(fwrite(entries,sizeof(entries),1,f)==1&&fclose(f)==0);
    char *name=getlogin();CHECK(name&&!strcmp(name,"abcdefgh"));
    for(int i=0;i<40;++i)CHECK(getlogin()==name&&!strcmp(name,"abcdefgh"));
    CHECK(close(0)==0&&ttyslot()==2);
    CHECK(close(1)==0&&ttyslot()==2&&getlogin()==name);
    CHECK(dup2(2,0)==0&&dup2(2,1)==1);
    memcpy(entries[2].ut_name,"root    ",8);
    f=fopen("/etc/utmp","w");CHECK(f);
    CHECK(fwrite(entries,sizeof(entries),1,f)==1&&fclose(f)==0);
    CHECK(getlogin()==name&&!strcmp(name,"root"));
    f=fopen("/etc/utmp","w");CHECK(f);
    CHECK(fwrite(entries,sizeof(entries)-1,1,f)==1&&fclose(f)==0);
    for(int i=0;i<40;++i)CHECK(getlogin()==NULL&&errno==EIO);
    CHECK(chmod("/etc/utmp",0600)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){if(setuid(123))_exit(1);_exit(getlogin()==NULL&&errno==EACCES?0:2);}
    int status;CHECK(wait(&status)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(unlink("/etc/utmp")==0&&unlink("/etc/ttys")==0);
    CHECK(unlink("/etc/..")==0&&unlink("/etc/.")==0&&unlink("/etc")==0);
    return 0;
}
int terminal_account_test(void)
{
    pid_t child=fork();if(child<0)return __LINE__;
    if(!child)_exit(accounts());
    int status;
    if(wait(&status)!=child||!WIFEXITED(status))return __LINE__;
    return WEXITSTATUS(status);
}
