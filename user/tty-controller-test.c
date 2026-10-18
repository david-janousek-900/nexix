#include <unistd.h>
#include <fcntl.h>
#include <sgtty.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define CHECK(c) do{if(!(c))return __LINE__%250+1;}while(0)
static long event(unsigned c,unsigned flags)
{long result=0x91f;__asm__ volatile("int $0x81":"+a"(result):"D"((uintptr_t)c),"S"((uintptr_t)flags):"memory","cc");return result;}
int tty_controller_test(void)
{
    unsigned char buffer[256];
    CHECK(open("/dev/dh0",O_RDWR)==-1 && errno==EBUSY);
    CHECK(close(0)==0 && close(1)==0 && close(2)==0);
    const char *nodes[]={"/dev/dc0","/dev/dh0","/dev/dz0","/dev/kl0"};
    for(unsigned profile=1;profile<=4;++profile){
        int fd=open(nodes[profile-1],O_RDWR);CHECK(fd==0);
        struct stat st;CHECK(fstat(fd,&st)==0 && st.st_rdev==NX_TTY_CONTROLLER_DEV(profile,0));
        CHECK(open(nodes[profile%4],O_RDWR)==-1 && errno==EBUSY);
        struct sgttyb s;CHECK(gtty(fd,&s)==0 && s.sg_erase=='#' && s.sg_kill=='@');
        s.sg_flags=RAW|(profile==NX_TTY_DC?ANYP:0);s.sg_ispeed=s.sg_ospeed=profile==NX_TTY_DC?B1200:B9600;
        CHECK(stty(fd,&s)==0 && event('A',NX_RX_PARITY)==0 && read(fd,buffer,1)==1 && buffer[0]=='A');
        if(profile==NX_TTY_DH || profile==NX_TTY_DZ){CHECK(event('x',NX_RX_BREAK|NX_RX_FRAME)==0 && read(fd,buffer,1)==1 && !buffer[0]);}
        if(profile==NX_TTY_DC){
            CHECK(event('x',NX_RX_FRAME)==0 && event('C',0)==0 && read(fd,buffer,1)==1 && buffer[0]=='C');
            s.sg_ispeed=s.sg_ospeed=B9600;CHECK(stty(fd,&s)==-1 && errno==EINVAL);
        }
        int ctty=open("/dev/tty",O_RDWR);CHECK(ctty>=0 && close(ctty)==0 && close(fd)==0);
    }
    CHECK(open("/dev/console",O_RDWR)==0 && dup(0)==1 && dup(0)==2);
    struct sgttyb saved,s;CHECK(gtty(0,&saved)==0);s=saved;s.sg_flags=LCASE;
    CHECK(stty(0,&s)==0 && event('A',0)==0 && event('\\',0)==0 && event('(',0)==0 && event('\n',0)==0);
    CHECK(read(0,buffer,sizeof(buffer))==3 && !memcmp(buffer,"a{\n",3));
    s.sg_flags=RAW|TANDEM;CHECK(stty(0,&s)==0);
    for(unsigned i=0;i<129;++i)CHECK(event('x',0)==0);
    CHECK(read(0,buffer,sizeof(buffer))==129);
    for(unsigned i=0;i<129;++i)CHECK(buffer[i]=='x');
    s.sg_flags=NL2;CHECK(stty(0,&s)==0);
    long before=event(256,0);CHECK(write(1,"\nR",2)==2 && stty(0,&s)==0 && event(256,0)-before>=10);
    CHECK(stty(0,&saved)==0);puts("NEXIX_TTY_CONTROLLERS_OK");return 0;
}
