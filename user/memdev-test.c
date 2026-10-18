#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/phys.h>
#include <sys/kinfo.h>
#include <nexix/memdev.h>
#include <sgtty.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"memdev line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static uintptr_t gate(long n,uintptr_t a,uintptr_t *second,uint64_t *value)
{
    uintptr_t d;uint64_t c;
    __asm__ volatile("int $0x81":"+a"(n),"=d"(d),"=c"(c):"D"(a):"memory","cc");
    if(second)*second=d;
    if(value)*value=c;
    return n;
}
static int collected(pid_t child)
{int s;return wait(&s)==child && WIFEXITED(s) && !WEXITSTATUS(s);}
int memdev_test(void)
{
    int mem=open("/dev/mem",O_RDWR),kmem=open("/dev/kmem",O_RDWR);CHECK(mem>=0 && kmem>=0);
    uintptr_t readonly;uint64_t original,value;
    uintptr_t probe=gate(0x91b,0,&readonly,&original);
    struct stat st;CHECK(fstat(mem,&st)==0 && S_ISCHR(st.st_mode) && st.st_rdev==NX_MEM_DEV && !st.st_size);
    CHECK(fstat(kmem,&st)==0 && st.st_rdev==NX_KMEM_DEV);
    CHECK(lseek(mem,probe,SEEK_SET)==(off_t)probe && read(mem,&value,8)==8 && value==original);
    uint64_t patch=UINT64_C(0xfeed9876543210ab);
    CHECK(lseek(kmem,probe,SEEK_SET)==(off_t)probe && write(kmem,&patch,8)==8);
    CHECK(gate(0x91b,0,NULL,&value)==probe && value==patch);
    CHECK(lseek(mem,probe,SEEK_SET)==(off_t)probe && write(mem,&original,8)==8);
    CHECK(gate(0x91b,0,NULL,&value)==probe && value==original);
    int alias=dup(mem);CHECK(alias>=0 && lseek(alias,0,SEEK_CUR)==(off_t)(probe+8));
    CHECK(lseek(alias,probe,SEEK_SET)==(off_t)probe && read(mem,&value,8)==8 && value==original);
    CHECK(lseek(alias,0,SEEK_CUR)==(off_t)(probe+8) && close(alias)==0);
    CHECK(lseek(mem,readonly,SEEK_SET)==(off_t)readonly && read(mem,&value,8)==8);
    CHECK(lseek(kmem,readonly,SEEK_SET)==(off_t)readonly && write(kmem,&value,8)==-1 && errno==EACCES);
    CHECK(lseek(kmem,0,SEEK_CUR)==(off_t)readonly);
    CHECK(lseek(mem,0,SEEK_SET)==0 && read(mem,&value,1)==-1 && errno==ENXIO);
    CHECK(lseek(mem,0xf0000,SEEK_SET)==0xf0000 && read(mem,&value,1)==-1 && errno==ENXIO);
    CHECK(lseek(mem,1UL<<30,SEEK_SET)==1UL<<30 && read(mem,&value,1)==-1 && errno==ENXIO);
    CHECK(lseek(mem,-1,SEEK_SET)==-1 && errno==EINVAL);
    CHECK(read(mem,(void *)1,1)==-1 && errno==EFAULT);
    CHECK(write(mem,(void *)1,1)==-1 && errno==EFAULT);
    CHECK(read(mem,(void *)1,0)==0 && write(mem,(void *)1,0)==0);
    CHECK(ioctl(mem,TIOCGETP,&st)==-1 && errno==ENOTTY);
    CHECK(ioctl(kmem,FIOCLEX,NULL)==0 && fcntl(kmem,F_GETFD)==FD_CLOEXEC);
    uintptr_t pa=gate(0x917,0,NULL,NULL);CHECK(pa && phys(0,2,pa)==0);
    unsigned char bytes[8]={0,0xff,2,3,0x80,5,6,7},got[8];
    CHECK(lseek(mem,pa+4093,SEEK_SET)==(off_t)(pa+4093) && write(mem,bytes,8)==8);
    CHECK(!memcmp((const void *)(NX_PHYS_ADDRESS(0)+4093),bytes,8));
    CHECK(lseek(kmem,pa+4093,SEEK_SET)==(off_t)(pa+4093) && read(kmem,got,8)==8 && !memcmp(got,bytes,8));
    pid_t child=fork();CHECK(child>=0);
    if(!child){CHECK(read(mem,got,1)==1);_exit(0);}
    CHECK(collected(child) && lseek(mem,0,SEEK_CUR)==(off_t)(pa+4102));
    CHECK(phys(0,0,0)==0);
    CHECK(chmod("/dev/mem",0666)==0 && chmod("/dev/kmem",0666)==0);
    child=fork();CHECK(child>=0);
    if(!child){
        CHECK(setuid(100)==0);
        CHECK(open("/dev/mem",O_RDONLY)==-1 && errno==EPERM);
        CHECK(open("/dev/kmem",O_WRONLY)==-1 && errno==EPERM);
        CHECK(read(mem,got,1)==-1 && errno==EPERM);
        CHECK(write(kmem,got,1)==-1 && errno==EPERM);
        int terminal=open("/dev/tty",O_RDWR);struct sgttyb settings;
        CHECK(terminal>=0 && gtty(terminal,&settings)==0 && close(terminal)==0);
        int n=open("/dev/null",O_RDWR);CHECK(n>=0 && write(n,"n",1)==1 && read(n,got,1)==0 && close(n)==0);
        _exit(0);
    }
    CHECK(collected(child));CHECK(chmod("/dev/mem",0600)==0 && chmod("/dev/kmem",0600)==0);
    CHECK(close(mem)==0 && close(kmem)==0);
    CHECK(mknod("/tmp/memory-null",S_IFCHR|0666,NX_MEMORY_NULL_DEV)==0);
    int n=open("/tmp/memory-null",O_RDWR|O_APPEND);CHECK(n>=0);
    CHECK(lseek(n,7,SEEK_SET)==7 && write(n,"discard",7)==7 && lseek(n,0,SEEK_CUR)==14);
    CHECK(read(n,got,8)==0 && lseek(n,0,SEEK_CUR)==14);
    CHECK(lseek(n,INT64_MAX,SEEK_SET)==INT64_MAX && write(n,"x",1)==-1 && errno==EINVAL);
    CHECK(lseek(n,0,SEEK_CUR)==INT64_MAX);
    CHECK(close(n)==0 && unlink("/tmp/memory-null")==0);
    CHECK(mknod("/tmp/memory-bad",S_IFCHR|0600,0x603)==0);
    CHECK(open("/tmp/memory-bad",O_RDONLY)==-1 && errno==ENXIO && unlink("/tmp/memory-bad")==0);
    int tty=open("/dev/tty",O_RDWR);CHECK(tty>=0);
    struct sgttyb saved,raw,observed;CHECK(gtty(0,&saved)==0);raw=saved;raw.sg_flags=RAW;
    CHECK(stty(tty,&raw)==0 && gtty(0,&observed)==0 && observed.sg_flags==RAW);
    CHECK(write(tty,"NEXIX_MEMDEV_TTY\r\n",18)==18);
    long feed=0x919;const unsigned char input[]={0,255,'r'};
    __asm__ volatile("int $0x81":"+a"(feed):"D"(input),"S"(sizeof(input)):"memory","cc");
    CHECK(!feed && read(tty,got,sizeof(got))==3 && !memcmp(got,input,3));
    CHECK(stty(tty,&saved)==0 && close(tty)==0);
    puts("NEXIX_MEMDEV_OK");return 0;
}
