#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <nexix/libc.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"profile line %d errno %d\n",__LINE__,errno);return __LINE__%250+1;}}while(0)
static unsigned short bins[32];
static const uintptr_t sample_pc=0x600000001000UL;
extern void profile_work(unsigned long);
extern long profile_gate(long,uintptr_t,uint64_t);
extern void profile_gate_return(void);
static int finished(pid_t pid,int code)
{int s;return wait(&s)==pid&&WIFEXITED(s)&&WEXITSTATUS(s)==code;}
static int burn(void)
{
    struct tms t;CHECK(times(&t)>=0);clock_t start=t.tms_utime;
    do{profile_work(100000);CHECK(times(&t)>=0);}while(t.tms_utime-start<3);
    return 0;
}
static int buckets(void)
{
    memset(bins,0,sizeof(bins));
    CHECK(profil(bins,8,sample_pc,65535)==0);
    CHECK(profile_gate(0x913,sample_pc+4,7)==0&&bins[2]==7&&bins[0]==0&&bins[1]==0&&bins[3]==0);
    CHECK(profile_gate(0x913,sample_pc+8,1)==0&&bins[2]==7&&bins[4]==0);
    CHECK(profile_gate(0x913,sample_pc-1,1)==0&&bins[0]==0);
    CHECK(profil(bins,1UL<<32,sample_pc,65535)==0);
    CHECK(profile_gate(0x913,sample_pc+4,3)==0&&bins[2]==10); /* Native size_t. */
    CHECK(profil(bins,7,sample_pc,65535)==0&&profile_gate(0x913,sample_pc+6,1)==0&&bins[3]==0);
    bins[0]=65534;CHECK(profil(bins,2,sample_pc,2)==0);
    CHECK(profile_gate(0x913,sample_pc,3)==0&&bins[0]==1);
    CHECK(profil(bins,2,sample_pc,65536)==-1&&errno==EINVAL);
    CHECK(profile_gate(0x912,0,0)==2&&profile_gate(0x913,sample_pc,5)==0&&bins[0]==6);
    CHECK(nx_syscall6(NX_SYS_PROFIL,0,0,0,1UL<<32,0,0)==-EINVAL&&profile_gate(0x912,0,0)==2);
    for(unsigned scale=0;scale<2;++scale){
        CHECK(profil(bins,2,sample_pc,scale)==0&&profile_gate(0x913,sample_pc,9)==0&&bins[0]==6);
    }
    CHECK(profil(bins,0,sample_pc,2)==0&&profile_gate(0x913,sample_pc,9)==0&&bins[0]==6);
    CHECK(profile_gate(0x912,0,0)==2); /* Size zero does not fault or change scale. */
    unsigned short stack_counter=0;
    CHECK((uintptr_t)&stack_counter>UINT32_MAX&&profil(&stack_counter,2,sample_pc,2)==0);
    CHECK(profile_gate(0x913,sample_pc,9)==0&&stack_counter==9); /* Native buffer pointer. */
    CHECK(profil(0,0,0,0)==0);
    return 0;
}
static int faults(void)
{
    uintptr_t bad[]={0,1,(uintptr_t)bins+1,UINTPTR_MAX-1,0xffff800000000000UL};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i){
        CHECK(profil((unsigned short *)bad[i],8,sample_pc,65535)==0);
        CHECK(profile_gate(0x912,0,0)==65535); /* Lazy buffer access. */
        errno=17;CHECK(profile_gate(0x913,sample_pc+(i==3?4:0),1)==0);
        CHECK(profile_gate(0x912,0,0)==0&&errno==17);
    }
    uintptr_t rx=(uintptr_t)profile_work&~1UL;unsigned short original;
    memcpy(&original,(void *)rx,2);
    CHECK(profil((unsigned short *)rx,2,sample_pc,2)==0&&profile_gate(0x913,sample_pc,1)==0);
    unsigned short after;memcpy(&after,(void *)rx,2);
    CHECK(after==original&&profile_gate(0x912,0,0)==0);
    /* A missing page after heap shrink must disable, without killing us. */
    void *old=sbrk(0);CHECK(old!=(void *)-1);
    uintptr_t page=((uintptr_t)old+4095)&~4095UL;CHECK(brk((void *)(page+4096))==0);
    CHECK(profil((unsigned short *)page,2,sample_pc,2)==0&&brk(old)==0);
    CHECK(profile_gate(0x913,sample_pc,1)==0&&profile_gate(0x912,0,0)==0);
    return 0;
}
static int lifecycle(void)
{
    bins[0]=12;CHECK(profil(bins,2,sample_pc,2)==0);
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        if(profile_gate(0x912,0,0)!=2||profile_gate(0x913,sample_pc,3)||bins[0]!=15)_exit(91);
        _exit(0);
    }
    CHECK(finished(pid,0)&&bins[0]==12&&profile_gate(0x912,0,0)==2);
    CHECK(profile_gate(0x913,sample_pc,5)==0&&bins[0]==17);
    CHECK(execv("/tmp/no-prof-image",(char *const[]){"no-prof",NULL})==-1&&errno==ENOENT);
    CHECK(profile_gate(0x912,0,0)==2&&profile_gate(0x913,sample_pc,2)==0&&bins[0]==19);
    CHECK(profile_gate(0x911,0,0)==0);
    pid=fork();CHECK(pid>=0);
    if(!pid){execl("/tmp/trace","trace","5","0","0",(char *)0);_exit(92);}
    CHECK(finished(pid,0)&&profile_gate(0x912,0,0)==2&&unlink("/tmp/trace")==0);
    CHECK(profil(0,0,0,0)==0);return 0;
}
int profile_test(void)
{
    int error=buckets();if(error)return error;
    error=faults();if(error)return error;
    error=lifecycle();if(error)return error;
    bins[0]=0;CHECK(profil(bins,2,(uintptr_t)profile_work,2)==0);
    error=burn();if(error)return error;
    CHECK(profil(0,0,0,0)==0&&bins[0]>0); /* Real user-mode PIT samples. */
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        bins[0]=0;
        if(setuid(123)||profil(bins,2,(uintptr_t)profile_work,2)||burn())_exit(93);
        if(profil(0,0,0,0))_exit(94);
        _exit(bins[0]>0?0:95);
    }
    CHECK(finished(pid,0)); /* Profiling is available without root. */
    bins[0]=0;CHECK(profil(bins,2,(uintptr_t)profile_gate_return,2)==0);
    CHECK(profile_gate(0x914,0,0)>=3&&bins[0]>=3); /* Trap-return system charge. */
    CHECK(profil(0,0,0,0)==0);
    bins[0]=0;CHECK(profil(bins,2,(uintptr_t)profile_gate_return,65535)==0);
    CHECK(profile_gate(0x915,0,0)==0); /* Elapsed blocked time is not system CPU. */
    CHECK(profil(0,0,0,0)==0&&bins[0]<3); /* Allow a user tick at the return PC. */
    CHECK(write(1,"NEXIX_PROFILE_OK\n",17)==17);return 0;
}
