/* V7 sys/acct.c syslock: retain the user lock state on resident native memory. */
#include "process.h"
#include "accounting.h"
#include <nexix/errno.h>
int proc_lock(uintptr_t flag)
{
    uintptr_t irq=proc_irq_save();
    struct proc *p=proc_current();
    int result=0;
    if(!p || !p->p_user || !proc_superuser(p->p_user))result=-NX_EPERM;
    else {
        p->p_flag&=~V7_SULOCK;
        if(flag)p->p_flag|=V7_SULOCK;
    }
    proc_irq_restore(irq);return result;
}
