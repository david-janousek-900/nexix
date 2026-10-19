#include "accounting.h"
#include "klog.h"
#include "console.h"
#include "process.h"
#include "uaccess.h"
#include <nexix/klog.h>
#include <nexix/errno.h>
int64_t klog_request(uintptr_t buffer,size_t capacity)
{
    static char snapshot[NX_KLOG_CAPACITY];
    uintptr_t irq=proc_irq_save();
    struct proc *p=proc_current();
    int64_t result=-NX_EPERM;
    if(!p || !p->p_user || !proc_superuser(p->p_user))goto done;
    size_t length=console_log_copy(snapshot,sizeof(snapshot));
    result=length;
    if(!buffer && !capacity)goto done;
    if(!buffer){result=-NX_EFAULT;goto done;}
    if(capacity<length){result=-NX_ERANGE;goto done;}
    int error=copyout(&p->p_vm,buffer,snapshot,length);
    if(error)result=error;
done:
    proc_irq_restore(irq);return result;
}
