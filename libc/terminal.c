/* Native adapters for V7 gen/ttyname.c, ttyslot.c and getlogin.c. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <utmp.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* Directories retain V7 disk records; utmp uses the installed native ABI. */
static ssize_t record_read(int fd, void *buffer, size_t size)
{
    size_t used=0;
    while(used<size){
        ssize_t n=read(fd,(char *)buffer+used,size-used);
        if(n<0){if(errno==EINTR)continue;return -1;}
        if(!n){if(used){errno=EIO;return -1;}return 0;}
        used+=(size_t)n;
    }
    return (ssize_t)used;
}
char *ttyname(int fd)
{
    static char path[sizeof("/dev/")+DIRSIZ];
    struct stat target,candidate;
    if(!isatty(fd)||fstat(fd,&target)<0)return NULL;
    if(!S_ISCHR(target.st_mode)){errno=ENOTTY;return NULL;}
    int directory=open("/dev",O_RDONLY);
    if(directory<0)return NULL;
    struct direct entry;char *answer=NULL;
    ssize_t n;
    while((n=record_read(directory,&entry,sizeof(entry)))>0){
        if(!entry.d_ino||entry.d_ino!=target.st_ino)continue;
        memcpy(path,"/dev/",5);
        size_t length=0;
        while(length<DIRSIZ&&entry.d_name[length])++length;
        memcpy(path+5,entry.d_name,length);path[5+length]=0;
        if(stat(path,&candidate)==0&&candidate.st_dev==target.st_dev&&candidate.st_ino==target.st_ino){answer=path;break;}
    }
    int error=errno;
    if(!answer&&n==0)error=ENOTTY;
    (void)close(directory);errno=error;return answer;
}
int ttyslot(void)
{
    char *terminal=ttyname(0);
    if(!terminal)terminal=ttyname(1);
    if(!terminal)terminal=ttyname(2);
    if(!terminal)return 0;
    char *name=strrchr(terminal,'/');name=name?name+1:terminal;
    int fd=open("/etc/ttys",O_RDONLY);
    if(fd<0)return 0;
    char line[32],byte;size_t used=0;int overflow=0,slot=0,answer=0;
    ssize_t n;
    while((n=record_read(fd,&byte,1))>0){
        if(byte!='\n'){
            if(used<sizeof(line)-1)line[used++]=byte;else overflow=1;
            continue;
        }
        if(slot==INT32_MAX){errno=ERANGE;break;}
        ++slot;line[used]=0;
        /* V7 names follow two control bytes; disabled lines still count. */
        if(!overflow&&used>=2&&!strcmp(line+2,name)){answer=slot;break;}
        used=0;overflow=0;
    }
    int error=errno;(void)close(fd);errno=error;return answer;
}
char *getlogin(void)
{
    static char name[9];
    int slot=ttyslot();if(!slot)return NULL;
    int fd=open("/etc/utmp",O_RDONLY);if(fd<0)return NULL;
    struct utmp entry;char *answer=NULL;
    if(lseek(fd,(off_t)slot*sizeof(entry),SEEK_SET)>=0&&record_read(fd,&entry,sizeof(entry))==(ssize_t)sizeof(entry)){
        size_t length=0;
        while(length<sizeof(entry.ut_name)&&entry.ut_name[length]&&entry.ut_name[length]!=' ')++length;
        memcpy(name,entry.ut_name,length);name[length]=0;answer=name;
    }
    int error=errno;(void)close(fd);errno=error;return answer;
}
