#include <nexix/dectape.h>
#include <nexix/disk.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"dectape line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static unsigned char bytes[512];static int ctl;
static int load(unsigned unit,const char *path,unsigned flags)
{struct nx_tape_load a={.family=NX_TAPE_TC,.unit=unit,.path=(uintptr_t)path,.flags=flags};return ioctl(ctl,NX_TAPE_LOAD,&a);}
static int unload(unsigned unit)
{struct nx_tape_load a={.family=NX_TAPE_TC,.unit=unit};return ioctl(ctl,NX_TAPE_UNLOAD,&a);}
static long gate(unsigned operation)
{long r=0x91d;__asm__ volatile("int $0x81":"+a"(r):"D"((uintptr_t)operation):"memory","cc");return r;}
static int directory(const char *path)
{char name[64];if(mknod(path,S_IFDIR|0755,0))return -1;snprintf(name,sizeof(name),"%s/.",path);if(link(path,name))return -1;snprintf(name,sizeof(name),"%s/..",path);return link("/tmp",name);}
static int remove_dir(const char *path)
{char name[64];snprintf(name,sizeof(name),"%s/.",path);if(unlink(name))return -1;snprintf(name,sizeof(name),"%s/..",path);if(unlink(name))return -1;return unlink(path);}
static unsigned word(unsigned at){return bytes[at]|((unsigned)bytes[at+1]<<8);}
static int persisted(void)
{
    long result=gate(2);if(result==-ENODEV || result==-ENXIO)return 0;CHECK(!result);
    CHECK(mknod("/tmp/tc-vol",S_IFBLK|0600,NX_DISK_DEV(NX_DISK_RP,2,0))==0 && directory("/tmp/tc-mnt")==0);
    CHECK(mount("/tmp/tc-vol","/tmp/tc-mnt",0)==0);
    CHECK(load(0,"/tmp/tc-mnt/tp",0)==0 && load(1,"/tmp/tc-mnt/fs",0)==0);
    int fd=open("/dev/tap0",O_RDWR);CHECK(fd>=0);
    CHECK(lseek(fd,512,SEEK_SET)==512 && read(fd,bytes,512)==512);
    unsigned sum=0;for(unsigned i=0;i<64;i+=2)sum+=word(i);
    CHECK(!(sum&65535) && !strcmp((char *)bytes,"hello") && word(44)==25 && bytes[37]==0 && word(38)==15);
    CHECK(lseek(fd,25*512,SEEK_SET)==25*512 && read(fd,bytes,15)==15 && !memcmp(bytes,"dectape-format\n",15));
    CHECK(lseek(fd,577*512,SEEK_SET)==577*512 && write(fd,"tc-last",7)==7 && close(fd)==0);
    CHECK(directory("/tmp/tc-fs")==0 && mount("/dev/tap1","/tmp/tc-fs",0)==0);
    CHECK(open("/dev/tap1",O_WRONLY)==-1 && errno==EBUSY && unload(1)==-1 && errno==EBUSY);
    fd=open("/tmp/tc-fs/hello",O_RDONLY);CHECK(fd>=0 && read(fd,bytes,512)==6 && !memcmp(bytes,"inner\n",6) && close(fd)==0);
    fd=creat("/tmp/tc-fs/persist",0644);CHECK(fd>=0 && write(fd,"tc-data",7)==7 && close(fd)==0);
    CHECK(umount("/tmp/tc-vol")==-1 && errno==EBUSY);
    CHECK(umount("/dev/tap1")==0 && remove_dir("/tmp/tc-fs")==0 && unload(1)==0 && unload(0)==0);
    struct stat st;CHECK(stat("/tmp/tc-mnt/blank",&st)==0);
    CHECK(load(2,"/tmp/tc-mnt/blank",st.st_size?0:NX_TAPE_INITIALIZE)==0);
    fd=open("/dev/tap2",O_RDWR);CHECK(fd>=0 && lseek(fd,577*512,SEEK_SET)==577*512);
    if(st.st_size){CHECK(read(fd,bytes,7)==7 && !memcmp(bytes,"tc-init",7));puts("NEXIX_DECTAPE_PERSIST_OK");}
    CHECK(lseek(fd,577*512,SEEK_SET)==577*512 && write(fd,"tc-init",7)==7 && close(fd)==0 && unload(2)==0);
    CHECK(umount("/tmp/tc-vol")==0 && remove_dir("/tmp/tc-mnt")==0 && unlink("/tmp/tc-vol")==0 && gate(1)==0);
    puts("NEXIX_DECTAPE_IDE_OK");return 0;
}
int dectape_test(void)
{
    ctl=open("/dev/tapectl",O_RDWR);CHECK(ctl>=0);
    CHECK(open("/dev/tap0",O_RDONLY)==-1 && errno==ENODEV);
    int fd=creat("/tmp/tc-image",0600);CHECK(fd>=0 && close(fd)==0);
    CHECK(load(0,"/tmp/tc-image",0)==-1 && errno==EINVAL);
    fd=open("/tmp/tc-image",O_WRONLY);CHECK(fd>=0 && lseek(fd,NX_TC_BYTES-1,SEEK_SET)==NX_TC_BYTES-1 && write(fd,"",1)==1 && close(fd)==0);
    for(unsigned unit=0;unit<8;++unit){
        CHECK(load(unit,"/tmp/tc-image",0)==0);
        CHECK(open("/tmp/tc-image",O_WRONLY)==-1 && errno==EBUSY);
        CHECK(load((unit+1)%8,"/tmp/tc-image",0)==-1 && errno==EBUSY);
        char path[24];snprintf(path,sizeof(path),"/dev/tap%u",unit);fd=open(path,O_RDWR);CHECK(fd>=0);
        struct stat st;CHECK(fstat(fd,&st)==0 && S_ISBLK(st.st_mode) && st.st_rdev==NX_TC_DEV(unit));
        CHECK(unload(unit)==-1 && errno==EBUSY);
        CHECK(lseek(fd,577*512,SEEK_SET)==577*512 && write(fd,"last",4)==4);
        CHECK(lseek(fd,513,SEEK_SET)==513 && write(fd,"random",6)==6 && lseek(fd,512,SEEK_SET)==512);
        CHECK(read(fd,bytes,8)==8 && !memcmp(bytes+1,"random",6));
        CHECK(lseek(fd,577*512,SEEK_SET)==577*512 && read(fd,bytes,512)==512 && !memcmp(bytes,"last",4));
        CHECK(read(fd,bytes,1)==-1 && errno==EIO && write(fd,"x",1)==-1 && errno==EIO);
        CHECK(ioctl(fd,NX_TAPE_OPERATION,bytes)==-1 && errno==ENOTTY);
        int d=dup(fd);CHECK(d>=0 && close(fd)==0 && unload(unit)==-1 && errno==EBUSY && close(d)==0 && unload(unit)==0);
    }
    CHECK(load(0,"/tmp/tc-image",NX_TAPE_READONLY)==0);
    CHECK(open("/dev/tap0",O_RDWR)==-1 && errno==EROFS);fd=open("/dev/tap0",O_RDONLY);CHECK(fd>=0);
    CHECK(unlink("/tmp/tc-image")==0 && lseek(fd,577*512,SEEK_SET)==577*512 && read(fd,bytes,4)==4 && !memcmp(bytes,"last",4));
    CHECK(close(fd)==0 && unload(0)==0);
    int error=persisted();if(error)return error;
    CHECK(close(ctl)==0);puts("NEXIX_DECTAPE_OK");return 0;
}
