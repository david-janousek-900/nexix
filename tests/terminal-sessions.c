/* Private init/session probe: starts without a controlling terminal, just like
 * the real init parent. No new session-management syscall is needed. */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sgtty.h>
static volatile sig_atomic_t interrupts;
static void caught(int n){if(n==SIGINT)++interrupts;}
static int identity(int line,pid_t group)
{
    struct stat a,b;int fd=open("/dev/tty",O_RDWR);
    if(fd<0 || fstat(0,&a) || fstat(fd,&b) || a.st_rdev!=(dev_t)(0x100+line) ||
        b.st_rdev!=0x200 || getpgrp()!=group)return 1;
    char marker[]="CTTY_0\n";marker[5]=(char)('0'+line);
    if(write(fd,marker,sizeof(marker)-1)!=sizeof(marker)-1 || close(fd))return 1;
    return 0;
}
static int session(int line)
{
    char tag[2]={(char)('0'+line),0},group[24],buffer[32];int status;
    if(identity(line,getpid()))return 10;
    snprintf(group,sizeof(group),"%d",getpgrp());
    pid_t pid=fork();if(pid<0)return 11;
    if(!pid){execl("/sbin/init","inherit",tag,group,(char *)0);_exit(12);}
    if(wait(&status)!=pid || status)return 13;
    struct sgttyb settings;
    if(gtty(0,&settings))return 14;
    settings.sg_flags=CRMOD;settings.sg_erase='#';settings.sg_kill='@';
    if(stty(0,&settings))return 15;
    printf("COOKED_%d\n",line);
    if(read(0,buffer,sizeof(buffer))!=3 || memcmp(buffer,"ac\n",3))return 16;
    printf("EMPTY_%d\n",line);
    if(read(0,buffer,sizeof(buffer))!=1 || buffer[0]!='\n')return 17;
    printf("EOF_%d\n",line);
    if(read(0,buffer,sizeof(buffer))!=0)return 18;
    if(signal(SIGINT,caught)==SIG_ERR)return 19;
    printf("SIGNAL_%d\n",line);
    if(read(0,buffer,sizeof(buffer))!=-1 || errno!=EINTR || interrupts!=1)return 20;
    printf("FLOW_%d\n",line);
    if(read(0,buffer,sizeof(buffer))!=3 || memcmp(buffer,"go\n",3) || interrupts!=1)return 21;
    printf("HELD_%d\nDONE_%d\n",line,line);
    return 0;
}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);
    if(argc==3 && !strcmp(argv[0],"inherit")){
        int line=atoi(argv[1]);
        if(identity(line,(pid_t)atoi(argv[2])))return 22;
        printf("INHERITED_%d\n",line);return 0;
    }
    if(argc==2)return session(atoi(argv[1]));
    if(getpid()!=1 || mknod("/tmp/line1",S_IFCHR|0600,0x101))return 23;
    pid_t children[2];
    for(int line=0;line<2;++line){
        children[line]=fork();if(children[line]<0)return 24;
        if(!children[line]){
            char tag[2]={(char)('0'+line),0};
            if(setpgrp()!=getpid() || open(line?"/tmp/line1":"/dev/console",O_RDWR)!=0 || dup(0)!=1 || dup(0)!=2)_exit(25);
            execl("/sbin/init","session",tag,(char *)0);_exit(26);
        }
    }
    int status,failed=0,results[2]={0};
    for(unsigned n=0;n<2;++n){pid_t p=wait(&status);results[n]=status;if((p!=children[0] && p!=children[1]) || status)failed=1;}
    if(open("/dev/console",O_RDWR)!=0 || dup(0)!=1 || dup(0)!=2)return 27;
    if(failed)printf("SESSION_ERRORS %d %d\n",results[0],results[1]);
    printf("SESSIONS_%s\n",failed?"FAILED":"PASSED");
    for(;;)pause();
}
