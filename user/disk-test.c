#include <nexix/disk.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"disk line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static unsigned char bytes[1024] __attribute__((aligned(2))),copy[1024] __attribute__((aligned(2)));
static long gate(unsigned operation,unsigned argument)
{long r=0x91c;__asm__ volatile("int $0x81":"+a"(r):"D"((uintptr_t)operation),"S"((uintptr_t)argument):"memory","cc");return r;}
static int native_ide(void)
{
    long result=gate(3,0);
    if(result==-ENODEV || result==-ENXIO){CHECK(!gate(1,0));return 0;}CHECK(!result);
    int signature=open("/dev/rp0",O_RDONLY);CHECK(signature>=0);
    CHECK(read(signature,bytes,512)==512 && close(signature)==0);
    const char tag[]="NEXIX_DISK_TEST_ONLY70";
    if(memcmp(bytes,tag,sizeof(tag)-1)){CHECK(!gate(1,0));return 0;}
    CHECK(mknod("/tmp/disk-volume",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_RP,2,0))==0);
    CHECK(mknod("/tmp/disk-raw",S_IFCHR|0600,NX_DISK_DEV(NX_DISK_RP,2,1))==0);
    CHECK(mknod("/tmp/disk-adj",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_RP,1,0))==0);
    CHECK(mknod("/tmp/disk-mount",S_IFDIR|0755,0)==0 && link("/tmp/disk-mount","/tmp/disk-mount/.")==0 && link("/tmp","/tmp/disk-mount/..")==0 && mount("/tmp/disk-volume","/tmp/disk-mount",0)==0);
    int fd=open("/tmp/disk-mount/hello",O_RDONLY);CHECK(fd>=0);
    CHECK(read(fd,bytes,sizeof(bytes))==18 && !memcmp(bytes,"controller-volume\n",18) && close(fd)==0);
    CHECK(open("/dev/rrp0",O_WRONLY)==-1 && errno==EBUSY);
    CHECK(open("/tmp/disk-raw",O_WRONLY)==-1 && errno==EBUSY);
    int adjacent=open("/tmp/disk-adj",O_RDWR);CHECK(adjacent>=0);
    CHECK(lseek(adjacent,512,SEEK_SET)==512 && write(adjacent,"adjacent",8)==8 && close(adjacent)==0);
    fd=open("/tmp/disk-mount/persist",O_RDONLY);
    if(fd>=0){CHECK(read(fd,bytes,sizeof(bytes))==15 && !memcmp(bytes,"controller-data",15) && close(fd)==0);puts("NEXIX_DISK_PERSIST_OK");}
    else CHECK(errno==ENOENT);
    fd=creat("/tmp/disk-mount/persist",0644);CHECK(fd>=0 && write(fd,"controller-data",15)==15);
    CHECK(umount("/tmp/disk-volume")==-1 && errno==EBUSY && close(fd)==0);
    CHECK(umount("/tmp/disk-volume")==0 && mount("/tmp/disk-volume","/tmp/disk-mount",0)==0);
    fd=open("/tmp/disk-mount/persist",O_RDONLY);CHECK(fd>=0 && read(fd,bytes,sizeof(bytes))==15 && !memcmp(bytes,"controller-data",15));
    CHECK(close(fd)==0 && umount("/tmp/disk-volume")==0 && unlink("/tmp/disk-mount/.")==0 && unlink("/tmp/disk-mount/..")==0 && unlink("/tmp/disk-mount")==0);
    CHECK(unlink("/tmp/disk-volume")==0 && unlink("/tmp/disk-raw")==0 && unlink("/tmp/disk-adj")==0);
    CHECK(gate(1,0)==0);puts("NEXIX_DISK_IDE_OK");return 0;
}
int disk_test(void)
{
    CHECK(!gate(0,0));const char *names[]={"hp","rf","rk","rl","rp"};
    unsigned lengths[]={9614,1024,4872,10240,81000};
    for(unsigned kind=0;kind<5;++kind){
        char block[24],raw[24];snprintf(block,sizeof(block),"/dev/%s0",names[kind]);snprintf(raw,sizeof(raw),"/dev/r%s0",names[kind]);
        int fd=open(block,O_RDWR),r=open(raw,O_RDWR);CHECK(fd>=0 && r>=0);struct stat st;
        CHECK(fstat(fd,&st)==0 && S_ISBLK(st.st_mode) && st.st_rdev==NX_DISK_DEV(kind,0,0) && !st.st_size);
        CHECK(read(fd,bytes,512)==512);for(unsigned i=0;i<512;++i)CHECK(bytes[i]==(unsigned char)i);
        memset(bytes,0x35+kind,512);CHECK(lseek(fd,(off_t)(lengths[kind]-1)*512,SEEK_SET)==(off_t)(lengths[kind]-1)*512 && write(fd,bytes,512)==512);
        CHECK(lseek(r,(off_t)(lengths[kind]-1)*512,SEEK_SET)==(off_t)(lengths[kind]-1)*512 && read(r,copy,512)==512 && !memcmp(bytes,copy,512));
        CHECK(read(fd,copy,2)==0 && write(fd,bytes,2)==-1 && errno==EIO);
        CHECK(read(r,copy+1,2)==-1 && errno==EFAULT);
        CHECK(lseek(fd,513,SEEK_SET)==513 && write(fd,"partial",7)==7);
        CHECK(lseek(r,519,SEEK_SET)==519 && read(r,copy,512)==512 && !memcmp(copy+1,"partial",7));
        int d=dup(fd);CHECK(d>=0 && lseek(d,0,SEEK_CUR)==520 && close(d)==0);
        CHECK(close(fd)==0 && close(r)==0);
        CHECK(mknod("/tmp/disk-bad",S_IFBLK|0600,NX_DISK_DEV(kind,255,0))==0);
        CHECK(open("/tmp/disk-bad",O_RDONLY)==-1 && errno==ENXIO && unlink("/tmp/disk-bad")==0);
        CHECK(!gate(0,0)); /* Reset sparse fixture before the next family. */
    }
    CHECK(!gate(2,1));CHECK(open("/dev/rk0",O_WRONLY)==-1 && errno==EROFS);CHECK(!gate(2,0));
    CHECK(!gate(1,0));CHECK(open("/dev/rk0",O_RDONLY)==-1 && errno==ENODEV);
    int error=native_ide();if(error)return error;
    puts("NEXIX_DISK_OK");return 0;
}
