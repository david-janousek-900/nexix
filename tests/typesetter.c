#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <nexix/output.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("typesetter failure line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
int main(void)
{
    unsigned char bytes[1027],copy[1027];
    for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=(unsigned char)i;
    CHECK(mknod("/tmp/catdev",S_IFCHR|0600,NX_CAT_DEV)==0);
    int capture=creat("/tmp/catcapture",0600);CHECK(capture>=0 && close(capture)==0);
    int fd=open("/tmp/catdev",O_WRONLY);CHECK(fd>=0);
    CHECK(write(fd,bytes,1)==-1 && errno==ENXIO);
    CHECK(open("/tmp/catdev",O_WRONLY)==-1 && errno==ENXIO);
    CHECK(ioctl(fd,NX_OUTPUT_ATTACH,(void *)1)==-1 && errno==EFAULT);
    CHECK(ioctl(fd,NX_OUTPUT_ATTACH,"/tmp/catdev")==-1 && errno==EINVAL);
    CHECK(ioctl(fd,NX_OUTPUT_ATTACH,"/tmp/catcapture")==0);
    CHECK(open("/tmp/catcapture",O_WRONLY)==-1 && errno==EBUSY);
    CHECK(ioctl(fd,NX_OUTPUT_ATTACH,"/tmp/catcapture")==-1 && errno==EBUSY);
    CHECK(ioctl(fd,0x1234,0)==-1 && errno==ENOTTY);
    CHECK(lseek(fd,0,0)==-1 && errno==ESPIPE);
    CHECK(write(fd,bytes,sizeof(bytes))==(int)sizeof(bytes));
    CHECK(read(fd,copy,1)==-1 && errno==EBADF);
    int duplicate=dup(fd);CHECK(duplicate>=0 && close(fd)==0);
    CHECK(open("/tmp/catdev",O_WRONLY)==-1 && errno==ENXIO);
    CHECK(close(duplicate)==0);
    capture=open("/tmp/catcapture",O_RDONLY);CHECK(capture>=0);
    CHECK(read(capture,copy,sizeof(copy))==(int)sizeof(copy) && !memcmp(bytes,copy,sizeof(bytes)));
    CHECK(close(capture)==0);
    fd=open("/tmp/catdev",O_WRONLY);CHECK(fd>=0);
    CHECK(ioctl(fd,NX_OUTPUT_ATTACH,"/tmp/catcapture")==-1 && errno==EBUSY);
    CHECK(close(fd)==0 && unlink("/tmp/catcapture")==0 && unlink("/tmp/catdev")==0);
    puts("TYPESETTER_OK");return 0;
}
