#include <mon.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"monitor line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
extern void monitor_site_a(void),monitor_site_b(void),monitor_site_c(void);
extern int monitor_abi(void);
static _Alignas(8) unsigned short manual[128];
static unsigned char header[64],record[16],content[256];
static uint64_t pcs[7];
static uint64_t get(const void *p,unsigned n)
{const unsigned char *bytes=p;uint64_t value=0;for(unsigned i=0;i<n;++i)value|=(uint64_t)bytes[i]<<(8*i);return value;}
static int read_exact(int fd,void *buffer,size_t size)
{
    unsigned char *p=buffer;
    while(size){ssize_t n=read(fd,p,size);if(n<=0)return 0;p+=n;size-=n;}return 1;
}
static int finished(pid_t pid,int code)
{int status;return wait(&status)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==code;}
static long gate(long number,uintptr_t argument)
{__asm__ volatile("int $0x81":"+a"(number):"D"(argument):"memory","cc");return number;}
static int manual_test(void)
{
    CHECK(monitor(NULL)==0&&access("mon.out",F_OK)==-1&&errno==ENOENT);
    memset(manual,0,sizeof(manual));manual[48]=123;
    CHECK(monitor((void *)0x40000000UL,(void *)0x800000000000UL,manual,(size_t)128,(size_t)2)==0);
    CHECK(countbase!=NULL);monitor_site_a();monitor_site_a();
    for(int i=0;i<3;++i)monitor_site_b();
    CHECK(countbase==NULL);monitor_site_c();monitor_site_a();CHECK(monitor_abi()==0);
    CHECK(manual[48]==123&&monitor(NULL)==0&&!countbase);
    int fd=open("mon.out",O_RDONLY);CHECK(fd>=0&&read_exact(fd,content,256)&&close(fd)==0);
    CHECK(!memcmp(content,NX_MON_MAGIC,8)&&get(content+44,4)==2&&get(content+48,8)==160);
    CHECK(get(content+72,8)==3&&get(content+88,8)==3&&get(content+96,2)==123);
    CHECK(unlink("mon.out")==0);
    memset(manual,0,sizeof(manual));
    CHECK(monitor((void *)0x600000001000UL,(void *)0x600000001008UL,manual,(size_t)40,(size_t)0)==0);
    long n=0x913;uintptr_t pc=0x600000001000UL,ticks=7;
    __asm__ volatile("int $0x81":"+a"(n):"D"(pc),"S"(ticks):"memory","cc");CHECK(n==0);
    CHECK(monitor(NULL)==0);
    fd=open("mon.out",O_RDONLY);CHECK(fd>=0&&read_exact(fd,content,80)&&close(fd)==0);
    CHECK(get(content+16,8)==pc&&get(content+24,8)==pc+8&&get(content+36,4)==65534&&get(content+64,2)==7);
    CHECK(unlink("mon.out")==0);return 0;
}
static int inspect(void)
{
    int fd=open("mon.out",O_RDONLY);CHECK(fd>=0&&read_exact(fd,header,64));
    CHECK(!memcmp(header,NX_MON_MAGIC,8)&&get(header+8,4)==1&&get(header+12,2)==62&&get(header+14,2)==64);
    uint64_t low=get(header+16,8),high=get(header+24,8),histogram=get(header+48,8);
    CHECK(low>=0x40000000UL&&high>low&&get(header+32,4)==100);
    CHECK(get(header+36,4)>1&&get(header+36,4)<=65534&&get(header+40,4)==300&&get(header+44,4)==7);
    CHECK(get(header+56,4)==16&&get(header+60,2)==2&&!get(header+62,2)&&histogram>0&&histogram%2==0);
    unsigned ones=0,fives=0,sevens=0;
    for(unsigned i=0;i<300;++i){
        CHECK(read_exact(fd,record,16));uint64_t pc=get(record,8),count=get(record+8,8);
        if(i<7){
            CHECK(pc>=low&&pc<high&&count>0);pcs[i]=pc;
            for(unsigned j=0;j<i;++j)CHECK(pcs[j]!=pc);
            if(count==1)++ones;else if(count==5)++fives;else if(count==7)++sevens;else CHECK(0);
        }else CHECK(pc==0&&count==0);
    }
    CHECK(ones==4&&fives==1&&sevens==2);
    uint64_t samples=0;
    for(uint64_t i=0;i<histogram;i+=2){unsigned short count;CHECK(read_exact(fd,&count,2));samples+=count;}
    CHECK(samples>0&&read(fd,record,1)==0&&close(fd)==0);
    struct stat st;CHECK(stat("mon.out",&st)==0&&st.st_size==(off_t)(64+300*16+histogram));
    return 0;
}
static int run(const char *mode,int code,int output)
{
    int fd=open("/tmp/mon-stdout",O_WRONLY|O_CREAT|O_TRUNC,0600);CHECK(fd>=0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        if(dup2(fd,1)<0||close(fd))_exit(99);
        char *args[]={"profiled",(char *)mode,NULL},*env[]={"MON_TEST=ok",NULL};
        execve("/tmp/monitor",args,env);_exit(98);
    }
    CHECK(close(fd)==0&&finished(pid,code));
    if(output){int error=inspect();if(error)return error;CHECK(unlink("mon.out")==0);}
    else CHECK(access("mon.out",F_OK)==-1&&errno==ENOENT);
    fd=open("/tmp/mon-stdout",O_RDONLY);CHECK(fd>=0);ssize_t n=read(fd,content,sizeof(content));CHECK(n>=0&&close(fd)==0);
    const char *expected=!strcmp(mode,"quick")?"":!strcmp(mode,"exec")?"replaced\n":"head\ntail\n";
    CHECK((size_t)n==strlen(expected)&&!memcmp(content,expected,n)&&unlink("/tmp/mon-stdout")==0);return 0;
}
int monitor_test(void)
{
    CHECK(chdir("/tmp")==0);
    int error=manual_test();if(error)return error;
    for(unsigned floating=0;floating<2;++floating){
        CHECK(gate(0x916,floating)==0);
        const char *modes[]={"return","exit","fork","quick","exec"};
        for(unsigned i=0;i<5;++i){error=run(modes[i],i==3?19:i==4?0:17,i<3);if(error)return error;}
        CHECK(unlink("/tmp/monitor")==0);
    }
    CHECK(chdir("/")==0);
    CHECK(write(1,"NEXIX_MONITOR_OK\n",17)==17);return 0;
}
