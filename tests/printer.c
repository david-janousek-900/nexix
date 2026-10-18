#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <nexix/output.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("printer failure line %d errno %d\n",__LINE__,errno);return 1;}}while(0)
static unsigned short word(const unsigned char *p){return p[0]|(p[1]<<8);}
int main(void)
{
    unsigned char bytes[600],capture[2048];
    for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=(unsigned char)i;
    CHECK(mknod("/tmp/vp",S_IFCHR|0600,NX_VP_DEV)==0);
    int fd=creat("/tmp/vpcapture",0600);CHECK(fd>=0 && close(fd)==0);
    fd=open("/tmp/vp",O_WRONLY);CHECK(fd>=0);
    unsigned short state=0;
    CHECK(ioctl(fd,NX_VGETSTATE,&state)==0 && state==(1|NX_VPRINT));
    CHECK(ioctl(fd,NX_VGETSTATE,(void *)1)==-1 && errno==EFAULT);
    CHECK(ioctl(fd,NX_OUTPUT_ATTACH,"/tmp/vpcapture")==0);
    state=NX_VPPLOT;CHECK(ioctl(fd,NX_VSETSTATE,&state)==0);
    CHECK(write(fd,bytes,600)==600);
    CHECK(ioctl(fd,NX_VGETSTATE,&state)==0 && state==(1|NX_VPLOT));
    state=NX_VPRINT|010|020;CHECK(ioctl(fd,NX_VSETSTATE,&state)==0);
    CHECK(close(fd)==0);
    fd=open("/tmp/vpcapture",O_RDONLY);CHECK(fd>=0);
    int size=read(fd,capture,sizeof(capture));CHECK(size==664 && close(fd)==0);
    CHECK(!memcmp(capture,"NXVP\1\0\0\0",8));
    unsigned offset=8;
    for(unsigned bit=2;bit<=020;bit<<=1)if(026&bit){
        CHECK(word(capture+offset)==NX_VP_COMMAND && word(capture+offset+2)==bit && word(capture+offset+4)==0);offset+=8;
    }
    CHECK(word(capture+offset)==NX_VP_DATA && word(capture+offset+2)==NX_VPPLOT && word(capture+offset+4)==512);
    CHECK(!memcmp(capture+offset+8,bytes,512));offset+=520;
    CHECK(word(capture+offset)==NX_VP_DATA && word(capture+offset+2)==NX_VPLOT && word(capture+offset+4)==88);
    CHECK(!memcmp(capture+offset+8,bytes+512,88));offset+=96;
    CHECK(word(capture+offset)==NX_VP_COMMAND && word(capture+offset+2)==010);offset+=8;
    CHECK(word(capture+offset)==NX_VP_COMMAND && word(capture+offset+2)==020);
    CHECK(unlink("/tmp/vpcapture")==0 && unlink("/tmp/vp")==0);
    CHECK(mknod("/tmp/vs",S_IFCHR|0600,NX_VS_DEV)==0);
    fd=creat("/tmp/vscapture",0600);CHECK(fd>=0 && close(fd)==0);
    fd=open("/tmp/vs",O_RDWR);CHECK(fd>=0 && ioctl(fd,NX_OUTPUT_ATTACH,"/tmp/vscapture")==0);
    struct nx_vs_input input={.count=11};
    for(unsigned i=0;i<11;++i)input.codes[i]=(unsigned char)(0xa0+i);
    CHECK(ioctl(fd,NX_VS_INPUT,&input)==0);
    input.count=1;CHECK(ioctl(fd,NX_VS_INPUT,&input)==-1 && errno==ENOSPC);
    for(unsigned i=0;i<11;++i){CHECK(read(fd,capture,8)==1 && capture[0]=="?0*#?546?213?879?"[i]);}
    input.count=12;CHECK(ioctl(fd,NX_VS_INPUT,&input)==-1 && errno==EINVAL);
    input.count=1;input.reserved=1;CHECK(ioctl(fd,NX_VS_INPUT,&input)==-1 && errno==EINVAL);
    CHECK(write(fd,bytes,241)==241 && close(fd)==0);
    fd=open("/tmp/vscapture",O_RDONLY);CHECK(fd>=0);
    size=read(fd,capture,sizeof(capture));CHECK(size==247 && close(fd)==0);
    offset=0;CHECK(capture[offset++]==026);
    for(unsigned i=0;i<241;++i){if(i%120==0)CHECK(capture[offset++]==026);CHECK(capture[offset++]==bytes[i]);}
    CHECK(capture[offset++]==026 && capture[offset++]==026 && offset==(unsigned)size);
    CHECK(unlink("/tmp/vscapture")==0 && unlink("/tmp/vs")==0);
    puts("PRINTER_OK");return 0;
}
