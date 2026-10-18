#include <nexix/tape.h>
#include <nexix/disk.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"tape line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static unsigned char data[1024] __attribute__((aligned(2)));
static int ctl;
static int attach(unsigned family,const char *path,unsigned flags,uint64_t capacity)
{struct nx_tape_load a={.family=family,.flags=flags,.path=(uintptr_t)path,.capacity=capacity};return ioctl(ctl,NX_TAPE_LOAD,&a);}
static int detach(unsigned family)
{struct nx_tape_load a={.family=family};return ioctl(ctl,NX_TAPE_UNLOAD,&a);}
static int op(int fd,unsigned command,unsigned count)
{struct nx_tape_operation a={command,count};return ioctl(fd,NX_TAPE_OPERATION,&a);}
static long gate(unsigned operation)
{long r=0x91d;__asm__ volatile("int $0x81":"+a"(r):"D"((uintptr_t)operation):"memory","cc");return r;}
static int persisted(void)
{
    long result=gate(0);if(result==-ENODEV || result==-ENXIO)return 0;CHECK(!result);
    CHECK(mknod("/tmp/tape-vol",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_RP,2,0))==0);
    CHECK(mknod("/tmp/tape-mnt",S_IFDIR|0755,0)==0 && link("/tmp/tape-mnt","/tmp/tape-mnt/.")==0 && link("/tmp","/tmp/tape-mnt/..")==0);
    CHECK(mount("/tmp/tape-vol","/tmp/tape-mnt",0)==0);
    int fd=open("/tmp/tape-mnt/image",O_RDONLY);
    if(fd>=0){
        CHECK(close(fd)==0 && attach(NX_TAPE_HT,"/tmp/tape-mnt/image",0,0)==0);
        fd=open("/dev/rht0",O_RDONLY);CHECK(fd>=0);
        CHECK(read(fd,data,512)==12 && !memcmp(data,"tape-persist",12) && read(fd,data,512)==0 && read(fd,data,512)==0);
        CHECK(close(fd)==0 && detach(NX_TAPE_HT)==0);puts("NEXIX_TAPE_PERSIST_OK");
    }else{
        CHECK(errno==ENOENT);fd=creat("/tmp/tape-mnt/image",0600);CHECK(fd>=0 && close(fd)==0);
        CHECK(attach(NX_TAPE_HT,"/tmp/tape-mnt/image",NX_TAPE_INITIALIZE,65536)==0);
        memcpy(data,"tape-persist",12);fd=open("/dev/rht0",O_WRONLY);CHECK(fd>=0 && write(fd,data,12)==12 && close(fd)==0 && detach(NX_TAPE_HT)==0);
    }
    CHECK(umount("/tmp/tape-vol")==0 && unlink("/tmp/tape-mnt/.")==0 && unlink("/tmp/tape-mnt/..")==0 && unlink("/tmp/tape-mnt")==0 && unlink("/tmp/tape-vol")==0);
    CHECK(!gate(1));puts("NEXIX_TAPE_IDE_OK");return 0;
}
int tape_test(void)
{
    ctl=open("/dev/tapectl",O_RDWR);CHECK(ctl>=0);
    CHECK(open("/dev/rht0",O_RDONLY)==-1 && errno==ENXIO);
    CHECK(attach(NX_TAPE_HT,"/tmp/tape-image",NX_TAPE_INITIALIZE,65536)==-1 && errno==ENOENT);
    int fd=creat("/tmp/tape-image",0600);CHECK(fd>=0);
    CHECK(attach(NX_TAPE_HT,"/tmp/tape-image",NX_TAPE_INITIALIZE,65536)==-1 && errno==EBUSY && close(fd)==0);
    CHECK(attach(NX_TAPE_HT,"/tmp/tape-image",NX_TAPE_INITIALIZE,65536)==0);
    CHECK(open("/tmp/tape-image",O_WRONLY)==-1 && errno==EBUSY);
    CHECK(attach(NX_TAPE_TM,"/tmp/tape-image",0,0)==-1 && errno==EBUSY);
    CHECK(mknod("/tmp/nrtape",S_IFCHR|0600,NX_TAPE_DEV(NX_TAPE_HT,NX_TAPE_NOREWIND|NX_TAPE_800,1))==0);
    fd=open("/tmp/nrtape",O_RDWR);CHECK(fd>=0);
    CHECK(open("/dev/ht0",O_RDONLY)==-1 && errno==ENXIO && detach(NX_TAPE_HT)==-1 && errno==EBUSY);
    struct nx_tape_status s;CHECK(ioctl(fd,NX_TAPE_STATUS,&s)==0 && s.density==800 && s.position==64 && s.records==0);
    CHECK(write(fd,data+1,2)==-1 && errno==EFAULT && write(fd,data,3)==-1 && errno==EFAULT);
    memcpy(data,"first!",6);CHECK(write(fd,data,6)==6 && op(fd,NX_TAPE_WRITE_MARK,1)==0);
    memcpy(data,"second",6);CHECK(write(fd,data,6)==6);
    CHECK(lseek(fd,0,SEEK_SET)==0 && read(fd,data,512)==0); /* raw seek has no transport effect */
    CHECK(op(fd,NX_TAPE_REWIND,1)==0 && read(fd,data,2)==-1 && errno==EIO);
    CHECK(read(fd,data,512)==0 && read(fd,data,512)==6 && !memcmp(data,"second",6));
    CHECK(op(fd,NX_TAPE_REWIND,1)==0 && op(fd,NX_TAPE_FORWARD_FILE,1)==0);
    CHECK(read(fd,data,512)==6 && !memcmp(data,"second",6));
    CHECK(op(fd,NX_TAPE_REWIND,1)==0);
    pid_t child=fork();CHECK(child>=0);
    if(!child){
        if(read(fd,data,512)!=6 || memcmp(data,"first!",6) || setuid(123)!=0 ||
           detach(NX_TAPE_HT)!=-1 || errno!=EPERM || close(fd)!=0)_exit(1);
        _exit(0);
    }
    int status;CHECK(wait(&status)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(read(fd,data,512)==0 && read(fd,data,512)==6 && !memcmp(data,"second",6));
    int d=dup(fd);CHECK(d>=0 && close(fd)==0 && ioctl(d,NX_TAPE_STATUS,&s)==0 && s.records==3);
    CHECK(close(d)==0);fd=open("/dev/rht0",O_RDONLY);CHECK(fd>=0);
    CHECK(ioctl(fd,NX_TAPE_STATUS,&s)==0 && s.record==4 && s.records==5 && s.density==1600);
    CHECK(read(fd,data,512)==0 && op(fd,NX_TAPE_WRITE_MARK,1)==-1 && errno==EBADF);
    CHECK(op(fd,NX_TAPE_REWIND,1)==0 && read(fd,data,512)==6 && !memcmp(data,"first!",6));
    CHECK(close(fd)==0 && detach(NX_TAPE_HT)==0);
    CHECK(attach(NX_TAPE_HT,"/tmp/tape-image",NX_TAPE_READONLY,0)==0);
    CHECK(open("/dev/rht0",O_RDWR)==-1 && errno==EROFS);
    fd=open("/dev/rht0",O_RDONLY);CHECK(fd>=0 && read(fd,data,512)==6 && close(fd)==0 && detach(NX_TAPE_HT)==0);
    /* Buffered 512-byte records support partial byte writes and seeks. */
    CHECK(attach(NX_TAPE_TM,"/tmp/tape-image",0,0)==0);fd=open("/dev/tm0",O_RDWR);CHECK(fd>=0);
    CHECK(op(fd,NX_TAPE_ERASE,1)==0 && write(fd,"abc",3)==3);
    CHECK(op(fd,NX_TAPE_FORWARD_RECORD,0)==-1 && errno==EINVAL && lseek(fd,0,SEEK_CUR)==3);
    CHECK(ioctl(fd,NX_TAPE_OPERATION,(void *)1)==-1 && errno==EFAULT && lseek(fd,0,SEEK_CUR)==3);
    CHECK(lseek(fd,1,SEEK_SET)==1 && write(fd,"Z",1)==1);
    CHECK(lseek(fd,0,SEEK_SET)==0 && read(fd,data,512)==512 && !memcmp(data,"aZc",3));
    for(unsigned i=3;i<512;++i)CHECK(!data[i]);
    CHECK(close(fd)==0 && detach(NX_TAPE_TM)==0);
    CHECK(attach(NX_TAPE_TM,"/tmp/tape-image",0,0)==0);fd=open("/dev/rtm0",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,data,512)==512 && !memcmp(data,"aZc",3) && read(fd,data,512)==0 && read(fd,data,512)==0);
    CHECK(close(fd)==0 && unlink("/tmp/tape-image")==0 && detach(NX_TAPE_TM)==0 && unlink("/tmp/nrtape")==0);
    /* An empty write-only close emits two marks; read/write without writes does not. */
    fd=creat("/tmp/tape-image",0600);CHECK(fd>=0 && close(fd)==0 && attach(NX_TAPE_HT,"/tmp/tape-image",NX_TAPE_INITIALIZE,96)==0);
    fd=open("/dev/rht0",O_RDWR);CHECK(fd>=0 && close(fd)==0);fd=open("/dev/rht0",O_RDONLY);CHECK(fd>=0);
    CHECK(ioctl(fd,NX_TAPE_STATUS,&s)==0 && !s.records && close(fd)==0);fd=open("/dev/rht0",O_WRONLY);CHECK(fd>=0 && close(fd)==0);
    fd=open("/dev/rht0",O_WRONLY);CHECK(fd>=0 && op(fd,NX_TAPE_FORWARD_RECORD,2)==0);
    CHECK(close(fd)==-1 && errno==ENOSPC); /* final-close errors survive inode release */
    CHECK(detach(NX_TAPE_HT)==0 && unlink("/tmp/tape-image")==0);
    int error=persisted();if(error)return error;
    CHECK(close(ctl)==0);puts("NEXIX_TAPE_OK");return 0;
}
