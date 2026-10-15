/* V7 sys4.c profil and conf/mch.s addupc, widened for native addresses. */
#include "profile.h"
#include "uaccess.h"
#include <nexix/errno.h>
int profile_configure(uintptr_t base,size_t size,uintptr_t offset,uint64_t scale)
{
    if(scale>UINT16_MAX)return -NX_EINVAL;
    struct user *u=proc_current()->p_user;
    u->u_prof.pr_base=base;u->u_prof.pr_size=size;
    u->u_prof.pr_off=offset;u->u_prof.pr_scale=scale;
    return 0;
}
void profile_sample(struct proc *p,uintptr_t pc,uint64_t ticks)
{
    struct user *u=p->p_user;
    /* profil(2) specifies 0/1 as off. The original assembler shifts both
     * operands first, then rounds the scaled byte index up to an even word. */
    if(!ticks||u->u_prof.pr_scale<2||pc<u->u_prof.pr_off)return;
    __uint128_t index=(((__uint128_t)((pc-u->u_prof.pr_off)>>1)*
                        (u->u_prof.pr_scale>>1))>>14)+1;
    index&=~(__uint128_t)1;
    if(index>=u->u_prof.pr_size||u->u_prof.pr_size-(size_t)index<2)return;
    uintptr_t base=u->u_prof.pr_base;
    if((size_t)index>UINTPTR_MAX-base)goto fault;
    uintptr_t address=base+(size_t)index;
    uint16_t counter;
    if(address%2||copyin(&p->p_vm,address,&counter,sizeof(counter)))goto fault;
    counter=(uint16_t)(counter+(uint16_t)ticks); /* V7 word counter wraps. */
    if(copyout(&p->p_vm,address,&counter,sizeof(counter)))goto fault;
    return;
fault:
    u->u_prof.pr_scale=0;
}
