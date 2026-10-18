/* Private hardware acceptance program, installed only on a disposable disk. */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sgtty.h>
int main(int argc,char **argv)
{
    (void)argv;
    unsigned char buffer[512];struct sgttyb settings;
    const char *path=argc==2?"/dev/ttyS1":"/tmp/uart2";
    if(argc==2){
        struct stat s;
        if(stat(path,&s) || s.st_mode!=(S_IFCHR|0600) || s.st_uid || s.st_gid || s.st_rdev!=0x101)return 1;
    }else if(mknod(path,S_IFCHR|0600,0x101))return 1;
    int fd=open(path,O_RDWR);
    if(fd<0 || gtty(fd,&settings))return 2;
    settings.sg_flags=RAW;
    if(stty(fd,&settings))return 3;
    if(write(fd,"COM2_READY\n",11)!=11)return 4;
    puts("COM1_READY");
    unsigned got=0;
    while(got<sizeof(buffer)){
        ssize_t n=read(fd,buffer+got,sizeof(buffer)-got);
        if(n<=0)return 5;
        got+=(unsigned)n;
    }
    for(unsigned i=0;i<sizeof(buffer);++i)if(buffer[i]!=(unsigned char)(i*37))return 6;
    for(unsigned i=0;i<sizeof(buffer);++i)buffer[i]=(unsigned char)(i*19);
    for(unsigned n=0;n<16;++n)if(write(fd,buffer,sizeof(buffer))!=sizeof(buffer))return 7;
    if(close(fd) || (argc!=2 && unlink(path)))return 8;
    puts("COM1_DONE");return 0;
}
