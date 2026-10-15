#include "profile.h"
#include <nexix/errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct proc process;
static struct user user;
static unsigned char memory[128];
static const uintptr_t base=0x500000000000UL;
static int reads,writes,read_error,write_error;
struct proc *proc_current(void){return &process;}
int copyin(const struct address_space *vm,uintptr_t from,void *to,size_t n)
{
    assert(vm==&process.p_vm&&n==2);++reads;
    if(read_error||from<base||from-base>sizeof(memory)-2)return -NX_EFAULT;
    memcpy(to,memory+(from-base),n);return 0;
}
int copyout(const struct address_space *vm,uintptr_t to,const void *from,size_t n)
{
    assert(vm==&process.p_vm&&n==2);++writes;
    if(write_error||to<base||to-base>sizeof(memory)-2)return -NX_EFAULT;
    memcpy(memory+(to-base),from,n);return 0;
}
static uint16_t counter(unsigned index)
{uint16_t value;memcpy(&value,memory+index,2);return value;}
static void reset(uintptr_t buffer,size_t size,uintptr_t offset,unsigned scale)
{memset(memory,0,sizeof(memory));reads=writes=read_error=write_error=0;assert(profile_configure(buffer,size,offset,scale)==0);}
int main(void)
{
    process.p_user=&user;user.u_error=17;
    uintptr_t pc=0x600000001000UL;
    /* Literal golden byte offsets from V7's shifts/multiply/rounding. */
    static const struct {unsigned scale,delta,index;} cases[]={
        {65535,0,0},{65535,1,0},{65535,2,2},{65535,3,2},{65535,4,4},
        {65534,6,6},{32767,2,0},{32767,4,2},{32768,6,4},
        {3,32768,2},{2,65534,2}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
        reset(base,sizeof(memory),pc,cases[i].scale);
        profile_sample(&process,pc+cases[i].delta,7);
        for(unsigned j=0;j<sizeof(memory);j+=2)assert(counter(j)==(j==cases[i].index?7:0));
        assert(reads==1&&writes==1&&user.u_error==17);
    }
    reset(base,8,pc,65535);profile_sample(&process,pc+8,1);assert(!reads&&!writes);
    profile_sample(&process,pc-1,1);assert(!reads&&!writes);
    profile_sample(&process,pc+65536,1);assert(!reads&&!writes);
    reset(base,7,pc,65535);profile_sample(&process,pc+6,1);assert(!reads&&!writes); /* Full word bounds. */
    reset(base,1,pc,2);profile_sample(&process,pc,1);assert(!reads&&!writes&&user.u_prof.pr_scale==2);
    for(unsigned scale=0;scale<2;++scale){reset(base,8,pc,scale);profile_sample(&process,pc,1);assert(!reads&&!writes);}
    reset(base,0,pc,2);profile_sample(&process,pc,1);assert(!reads&&!writes&&user.u_prof.pr_scale==2);
    reset(base,8,pc,2);profile_sample(&process,pc,0);assert(!reads&&!writes);
    uint16_t initial=65534;memcpy(memory,&initial,2);
    profile_sample(&process,pc,3);assert(counter(0)==1);
    profile_sample(&process,pc,65536+5);assert(counter(0)==6);
    assert(profile_configure(base+4,4,pc+4,65536)==-NX_EINVAL);
    assert(user.u_prof.pr_base==base&&user.u_prof.pr_size==8&&user.u_prof.pr_off==pc&&user.u_prof.pr_scale==2);
    assert(profile_configure(0,0,0,UINT64_MAX)==-NX_EINVAL&&user.u_prof.pr_scale==2);
    reset(base,sizeof(memory),0,65535);
    profile_sample(&process,UINTPTR_MAX,1);assert(!reads&&!writes); /* No wrapped multiply/index. */
    reset(UINTPTR_MAX-1,SIZE_MAX,pc,65535);profile_sample(&process,pc+4,1);
    assert(!reads&&!writes&&!user.u_prof.pr_scale); /* Buffer address overflow. */
    reset(base+1,4,pc,2);profile_sample(&process,pc,1);assert(!reads&&!writes&&!user.u_prof.pr_scale);
    reset(base,8,pc,2);read_error=1;profile_sample(&process,pc,1);
    assert(reads==1&&!writes&&!user.u_prof.pr_scale&&user.u_error==17);
    read_error=0;profile_sample(&process,pc,1);assert(reads==1&&!writes); /* Stays disabled. */
    reset(base,8,pc,2);write_error=1;profile_sample(&process,pc,1);
    assert(reads==1&&writes==1&&!user.u_prof.pr_scale&&counter(0)==0&&user.u_error==17);
    puts("PASS: profil V7 golden buckets, 64-bit bounds/overflow, counter wrap, off/zero-size modes, lazy faults and errno preservation");
}
