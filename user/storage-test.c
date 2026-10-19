#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <string.h>
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
static unsigned short data[600],other[600];
int storage_test(void)
{
    CHECK(mknod("/tmp/blk",S_IFBLK|0600,NX_ROOT_BLOCK_DEV)==0);
    CHECK(mknod("/tmp/raw",S_IFCHR|0600,NX_ROOT_RAW_DEV)==0);
    CHECK(mknod("/tmp/baddev",S_IFBLK|0600,0x0100)==0);
    CHECK(open("/tmp/baddev",O_RDONLY)==-1&&errno==ENODEV);
    CHECK(open("/tmp/blk",O_WRONLY)==-1&&errno==EBUSY);
    CHECK(open("/tmp/raw",O_RDWR)==-1&&errno==EBUSY);
    pid_t child=fork();CHECK(child>=0);
    if(!child){if(setuid(123))_exit(99);int fd=open("/tmp/blk",O_RDONLY);_exit(fd==-1&&errno==EACCES?0:98);}
    int status;CHECK(wait(&status)==child&&status==0);
    int b=open("/tmp/blk",O_RDONLY),r=open("/tmp/raw",O_RDONLY);CHECK(b>=0&&r>=0);
    struct stat st;CHECK(fstat(b,&st)==0&&S_ISBLK(st.st_mode)&&st.st_rdev==NX_ROOT_BLOCK_DEV&&st.st_size==0);
    CHECK(read(b,data,1024)==1024&&read(r,other,1024)==1024&&!memcmp(data,other,1024));
    off_t end=(((uint32_t)data[257]<<16)|data[258])*512; /* V7 superblock fsize. */
    CHECK(end>1024);
    CHECK(write(b,data,2)==-1&&errno==EBADF);
    CHECK(lseek(b,511,SEEK_SET)==511&&read(b,data,3)==3);
    CHECK(lseek(b,0,SEEK_SET)==0&&read(b,other,2)==2);
    CHECK(lseek(r,1,SEEK_SET)==1&&read(r,data,2)==2&&!memcmp(data,other,2)&&lseek(r,0,SEEK_CUR)==3);
    CHECK(lseek(r,0,SEEK_SET)==0&&read(r,(char *)data+1,2)==-1&&errno==EFAULT);
    CHECK(read(r,data,3)==-1&&errno==EFAULT);
    CHECK(read(r,(void *)1,2)==-1&&errno==EFAULT);
    CHECK(read(r,(void *)1,0)==0);
    CHECK(fcntl(b,F_SETFL,O_APPEND)==0&&fcntl(b,F_GETFL)==O_APPEND);
    int duplicate=dup(b);CHECK(duplicate>=0&&lseek(b,0,SEEK_SET)==0);
    CHECK(read(duplicate,data,2)==2&&lseek(b,0,SEEK_CUR)==2);
    CHECK(lseek(b,end,SEEK_SET)==end&&read(b,data,2)==0);
    CHECK(lseek(b,end+1,SEEK_SET)==end+1&&read(b,data,2)==-1&&errno==EIO);
    CHECK(lseek(b,end-1,SEEK_SET)==end-1&&read(b,data,2)==1);
    CHECK(ioctl(b,0,0)==-1&&errno==ENOTTY);
    CHECK(close(b)==0&&close(duplicate)==0&&close(r)==0);
    CHECK(unlink("/tmp/blk")==0&&unlink("/tmp/raw")==0&&unlink("/tmp/baddev")==0);
    CHECK(mknod("/tmp/blk",S_IFBLK|0600,1)==0);
    CHECK(mknod("/tmp/raw",S_IFCHR|0600,0x0401)==0);
    CHECK(mknod("/tmp/baddev",S_IFCHR|0600,0x0402)==0);
    CHECK(open("/tmp/baddev",O_WRONLY)==-1&&errno==EROFS);
    b=open("/tmp/blk",O_RDWR);r=open("/tmp/raw",O_RDWR);CHECK(b>=0&&r>=0);
    memset(data,0x5a,sizeof(data));
    CHECK(lseek(b,511,SEEK_SET)==511&&write(b,data,515)==515);
    CHECK(lseek(r,512,SEEK_SET)==512&&read(r,other,512)==512&&!memcmp(data,other,512));
    CHECK(lseek(r,0,SEEK_SET)==0&&write(r,data,1024)==1024);
    CHECK(lseek(b,0,SEEK_SET)==0&&read(b,other,1024)==1024&&!memcmp(data,other,1024));
    CHECK(lseek(b,1535,SEEK_SET)==1535&&write(b,data,2)==1);
    CHECK(write(b,data,2)==-1&&errno==EIO);
    CHECK(close(b)==0&&close(r)==0);
    b=open("/tmp/blk",O_WRONLY);CHECK(b>=0&&read(b,data,2)==-1&&errno==EBADF&&close(b)==0);
    CHECK(unlink("/tmp/blk")==0&&unlink("/tmp/raw")==0&&unlink("/tmp/baddev")==0);
    const char marker[]="NEXIX_STORAGE_OK\n";
    CHECK(write(1,marker,sizeof(marker)-1)==sizeof(marker)-1);
    return 0;
}
