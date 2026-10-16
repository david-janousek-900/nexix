#include "process.h"
#include <nexix/acct.h>
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
static struct proc p;
static struct user u;
static bool present=true;
static int locked,saves,restores;
uintptr_t proc_irq_save(void){assert(!locked);locked=1;++saves;return 0x200;}
void proc_irq_restore(uintptr_t flags){assert(locked && flags==0x200);locked=0;++restores;}
struct proc *proc_current(void){assert(locked);return present?&p:NULL;}
int main(void)
{
    p.p_user=&u;u.u_ruid=123;u.u_acflag=NX_AFORK;
    /* Exercise all unrelated flag combinations using the production suser helper. */
    for(unsigned flags=0;flags<=255;++flags){
        p.p_flag=flags;
        assert(proc_lock(0)==0 && p.p_flag==(flags&~V7_SULOCK));
        assert(proc_lock(1)==0 && p.p_flag==(flags|V7_SULOCK));
        assert(proc_lock(UINTPTR_MAX)==0 && p.p_flag==(flags|V7_SULOCK));
        assert(proc_lock(UINT64_C(1)<<32)==0 && p.p_flag==(flags|V7_SULOCK));
        assert(proc_lock(0)==0 && p.p_flag==(flags&~V7_SULOCK));
    }
    assert(u.u_acflag==(NX_AFORK|NX_ASU)); /* Effective root, non-root real UID. */
    u.u_uid=123;u.u_ruid=0;
    for(unsigned flags=0;flags<=255;++flags){
        p.p_flag=flags;u.u_acflag=NX_AFORK;
        assert(proc_lock(0)==-NX_EPERM && p.p_flag==flags && u.u_acflag==NX_AFORK);
        assert(proc_lock(1)==-NX_EPERM && p.p_flag==flags && u.u_acflag==NX_AFORK);
    }
    present=false;assert(proc_lock(1)==-NX_EPERM);
    present=true;p.p_user=NULL;assert(proc_lock(0)==-NX_EPERM);
    assert(!locked && saves==restores);
    puts("PASS: V7 lock truth values, effective-ID permissions, ASU, flag preservation and IRQ restoration");
    return 0;
}
