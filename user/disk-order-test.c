#include <nexix/disk.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"ordering line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static unsigned char data[1024],copy[1024];
static long gate(unsigned operation,unsigned value)
{long r=0x91e;__asm__ volatile("int $0x81":"+a"(r):"D"((uintptr_t)operation),"S"((uintptr_t)value):"memory","cc");return r;}
static int persisted(void)
{
    long result=gate(2,0);if(result==-ENODEV || result==-ENXIO)return 0;CHECK(!result && !gate(3,0));
    CHECK(mknod("/tmp/order-volume",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_HP,NX_HP_INTERLEAVED_MINOR(2,1),0))==0);
    CHECK(mknod("/tmp/order-member",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_HP,9,0))==0);
    CHECK(mknod("/tmp/order-mnt",S_IFDIR|0755,0)==0 && link("/tmp/order-mnt","/tmp/order-mnt/.")==0 && link("/tmp","/tmp/order-mnt/..")==0);
    int writer=open("/tmp/order-volume",O_RDWR);CHECK(writer>=0 && mount("/tmp/order-volume","/tmp/order-mnt",0)==0);
    CHECK(write(writer,"x",1)==-1 && errno==EBUSY && close(writer)==0);
    CHECK(open("/tmp/order-member",O_WRONLY)==-1 && errno==EBUSY);
    int fd=open("/tmp/order-mnt/hello",O_RDONLY);CHECK(fd>=0 && read(fd,data,1024)==16 && !memcmp(data,"ordering-volume\n",16) && close(fd)==0);
    for(unsigned i=0;i<700;++i)data[i]=(unsigned char)i;
    fd=open("/tmp/order-mnt/persist",O_RDONLY);
    if(fd>=0){CHECK(read(fd,copy,1024)==700 && !memcmp(copy,data,700) && close(fd)==0);puts("NEXIX_ORDER_PERSIST_OK");}else CHECK(errno==ENOENT);
    fd=creat("/tmp/order-mnt/persist",0644);CHECK(fd>=0 && write(fd,data,700)==700 && close(fd)==0);
    CHECK(umount("/tmp/order-volume")==0 && unlink("/tmp/order-mnt/.")==0 && unlink("/tmp/order-mnt/..")==0 && unlink("/tmp/order-mnt")==0);
    CHECK(unlink("/tmp/order-volume")==0 && unlink("/tmp/order-member")==0 && !gate(1,0));puts("NEXIX_ORDER_IDE_OK");return 0;
}
int disk_order_test(void)
{
    CHECK(!gate(0,0) && !gate(3,0));
    unsigned minor=NX_HP_INTERLEAVED_MINOR(2,0);
    CHECK(mknod("/tmp/hp-striped",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_HP,minor,0))==0);
    CHECK(mknod("/tmp/hp-member",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_HP,8,0))==0);
    CHECK(mknod("/tmp/hp-raw",S_IFCHR|0600,NX_DISK_DEV(NX_DISK_HP,minor,1))==0);
    CHECK(open("/tmp/hp-raw",O_RDONLY)==-1 && errno==ENXIO);
    int fd=open("/tmp/hp-striped",O_RDWR),a=open("/dev/hp0",O_RDONLY),b=open("/tmp/hp-member",O_RDONLY);CHECK(fd>=0 && a>=0 && b>=0);
    memset(data,0x31,512);memset(data+512,0x72,512);CHECK(write(fd,data,1024)==1024);
    CHECK(read(a,copy,512)==512 && !memcmp(data,copy,512) && read(b,copy,512)==512 && !memcmp(data+512,copy,512));
    CHECK(lseek(fd,513,SEEK_SET)==513 && write(fd,"stripe",6)==6);
    CHECK(lseek(b,0,SEEK_SET)==0 && read(b,copy,512)==512 && !memcmp(copy+1,"stripe",6));
    CHECK(lseek(fd,19227L*512,SEEK_SET)==19227L*512 && read(fd,copy,512)==512);
    for(unsigned i=0;i<512;++i)CHECK(copy[i]==(unsigned char)(9613+31+i));
    CHECK(read(fd,copy,1)==0 && write(fd,"x",1)==-1 && errno==EIO);
    CHECK(!gate(4,1) && lseek(fd,0,SEEK_SET)==0 && write(fd,"x",1)==-1 && errno==EROFS && !gate(4,0));
    CHECK(close(fd)==0 && close(a)==0 && close(b)==0 && unlink("/tmp/hp-striped")==0 && unlink("/tmp/hp-member")==0 && unlink("/tmp/hp-raw")==0 && !gate(1,0));
    int error=persisted();if(error)return error;
    puts("NEXIX_DISK_ORDER_OK");return 0;
}
