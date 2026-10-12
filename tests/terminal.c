/* Inject historical record boundaries and descriptor failures independently
 * of the native kernel; the QEMU fixture covers real filesystem consumers. */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <utmp.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do {if(!(c))return __LINE__;}while(0)
int errno;
static struct direct directory[3];
static struct utmp accounts[4];
static const char *ttys;
static size_t positions[3],account_size;
static int live,terminal_fd,missing,read_error,seek_error,interrupt,mismatch;
int isatty(int fd)
{if(fd==terminal_fd)return 1;errno=fd<0?EBADF:ENOTTY;return 0;}
int fstat(int fd,struct stat *s)
{(void)fd;*s=(struct stat){.st_mode=S_IFCHR,.st_dev=7,.st_ino=42};return 0;}
int stat(const char *path,struct stat *s)
{(void)path;*s=(struct stat){.st_mode=S_IFCHR,.st_dev=mismatch?8:7,.st_ino=42};return 0;}
int open(const char *path,int flags,...)
{
    (void)flags;int fd;
    if(!strcmp(path,"/dev"))fd=10;
    else if(!strcmp(path,"/etc/ttys"))fd=11;
    else if(!strcmp(path,"/etc/utmp"))fd=12;
    else{errno=ENOENT;return -1;}
    if(fd==missing){errno=EACCES;return -1;}
    positions[fd-10]=0;++live;return fd;
}
int close(int fd){(void)fd;--live;return 0;}
off_t lseek(int fd,off_t offset,int whence)
{
    if(fd!=12||whence!=SEEK_SET||offset<0||seek_error){errno=EIO;return -1;}
    positions[2]=(size_t)offset;return offset;
}
ssize_t read(int fd,void *buffer,size_t count)
{
    if(interrupt){interrupt=0;errno=EINTR;return -1;}
    if(fd==read_error){errno=EIO;return -1;}
    const char *data;size_t size;
    if(fd==10){data=(const char *)directory;size=sizeof(directory);}
    else if(fd==11){data=ttys;size=strlen(ttys);}
    else{data=(const char *)accounts;size=account_size;}
    size_t *position=&positions[fd-10];
    if(*position>=size)return 0;
    if(count>size-*position)count=size-*position;
    if(count>3)count=3; /* Legal short reads, including native utmp records. */
    memcpy(buffer,data+*position,count);*position+=count;return (ssize_t)count;
}
static void reset(void)
{
    memset(directory,0,sizeof(directory));directory[1].d_ino=41;
    directory[2].d_ino=42;memcpy(directory[2].d_name,"tty01234567890",DIRSIZ);
    memset(accounts,0,sizeof(accounts));memcpy(accounts[2].ut_name,"abcdefgh",8);
    accounts[2].ut_time=123456789;account_size=sizeof(accounts);
    ttys="00other\n00tty01234567890\n";
    terminal_fd=0;missing=read_error=seek_error=interrupt=mismatch=0;
}
int main(void)
{
    reset();char *name=ttyname(0);CHECK(name&&!strcmp(name,"/dev/tty01234567890")&&live==0);
    interrupt=1;CHECK(ttyname(0)==name&&live==0);
    CHECK(ttyname(-1)==NULL&&errno==EBADF&&live==0);
    mismatch=1;CHECK(ttyname(0)==NULL&&errno==ENOTTY&&live==0);mismatch=0;
    missing=10;CHECK(ttyname(0)==NULL&&errno==EACCES&&live==0);missing=0;
    read_error=10;CHECK(ttyname(0)==NULL&&errno==EIO&&live==0);read_error=0;
    CHECK(ttyslot()==2&&live==0);
    terminal_fd=1;CHECK(ttyslot()==2&&live==0);
    terminal_fd=2;CHECK(ttyslot()==2&&live==0);
    terminal_fd=3;CHECK(ttyslot()==0&&live==0);terminal_fd=0;
    ttys="\nx\n00tty01234567890\n";CHECK(ttyslot()==3);
    ttys="00tty01234567890";CHECK(ttyslot()==0); /* V7 requires newline. */
    char longline[120];memset(longline,'x',80);
    strcpy(longline+80,"\n00tty01234567890\n");ttys=longline;CHECK(ttyslot()==2&&live==0);
    reset();name=getlogin();CHECK(name&&!strcmp(name,"abcdefgh")&&live==0);
    CHECK(accounts[2].ut_time==123456789);
    memcpy(accounts[2].ut_name,"ab cd   ",8);CHECK(getlogin()==name&&!strcmp(name,"ab"));
    memset(accounts[2].ut_name,0,8);CHECK(getlogin()==name&&!strcmp(name,""));
    missing=12;CHECK(getlogin()==NULL&&errno==EACCES&&live==0);missing=0;
    seek_error=1;CHECK(getlogin()==NULL&&errno==EIO&&live==0);seek_error=0;
    read_error=12;CHECK(getlogin()==NULL&&errno==EIO&&live==0);read_error=0;
    account_size=3*sizeof(struct utmp)-1;
    for(int i=0;i<40;++i)CHECK(getlogin()==NULL&&errno==EIO&&live==0);
    reset();missing=11;CHECK(ttyslot()==0&&getlogin()==NULL&&errno==EACCES&&live==0);
    return 0;
}
