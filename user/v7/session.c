#include "session.h"
#include <unistd.h>
#include <fcntl.h>
#include <utmp.h>
#include <string.h>
#include <time.h>
#include <errno.h>
static void field(char output[8],const char *input)
{unsigned n=0;while(n<8 && input[n]){output[n]=input[n];++n;}while(n<8)output[n++]=0;}
static int put(int fd,const struct utmp *record)
{
    size_t n=0;
    while(n<sizeof(*record)){
        ssize_t done=write(fd,(const char *)record+n,sizeof(*record)-n);
        if(done<0 && errno==EINTR)continue;
        if(done<=0)return -1;
        n+=(size_t)done;
    }
    return 0;
}
static int get(int fd,struct utmp *record)
{
    size_t n=0;
    while(n<sizeof(*record)){
        ssize_t done=read(fd,(char *)record+n,sizeof(*record)-n);
        if(done<0 && errno==EINTR)continue;
        if(done<0)return -1;
        if(!done)return n?-1:0;
        n+=(size_t)done;
    }
    return 1;
}
static int append(const struct utmp *record)
{
    int fd=open("/usr/adm/wtmp",O_WRONLY|O_APPEND);
    if(fd<0)return -1;
    int result=put(fd,record);if(close(fd))result=-1;return result;
}
int v7_accounting_reset(void)
{int fd=creat("/etc/utmp",0644);return fd<0?-1:close(fd);}
int v7_login_record(const char *name,const char *terminal,int slot)
{
    if(slot<=0)return 0;
    struct utmp record={0};const char *base=strrchr(terminal,'/');
    field(record.ut_line,base?base+1:terminal);field(record.ut_name,name);
    record.ut_time=time(NULL);
    int result=0,fd=open("/etc/utmp",O_WRONLY);
    if(fd<0)result=-1;
    else{
        if(lseek(fd,(off_t)slot*sizeof(record),SEEK_SET)<0 || put(fd,&record))result=-1;
        if(close(fd))result=-1;
    }
    if(append(&record))result=-1;
    return result;
}
int v7_logout_record(const char *line)
{
    struct utmp record,logout={0};int result=0,fd=open("/etc/utmp",O_RDWR);
    field(logout.ut_line,line);logout.ut_time=time(NULL);
    if(fd<0)result=-1;
    else{
        int state;
        while((state=get(fd,&record))>0){
            if(memcmp(record.ut_line,logout.ut_line,8))continue;
            memset(record.ut_name,0,8);record.ut_time=logout.ut_time;
            if(lseek(fd,-(off_t)sizeof(record),SEEK_CUR)<0 || put(fd,&record)){result=-1;break;}
        }
        if(state<0)result=-1;
        if(close(fd))result=-1;
    }
    if(append(&logout))result=-1;
    return result;
}
