#include "utsname.h"
#include "version.h"
#include "process.h"
#include "accounting.h"
#include "uaccess.h"
#include <nexix/hostname.h>
#include <nexix/errno.h>
static struct nx_utsname identity = {
    NEXIX_SYSNAME,NEXIX_NODENAME,NEXIX_RELEASE,NEXIX_VERSION,NEXIX_MACHINE
};
int64_t uname_request(uintptr_t buffer)
{
    uintptr_t irq=proc_irq_save();
    int error=copyout(&proc_current()->p_vm,buffer,&identity,sizeof identity);
    proc_irq_restore(irq);return error;
}
int64_t hostname_request(uintptr_t buffer,size_t length)
{
    char name[NX_UTS_FIELD_SIZE]={0};
    uintptr_t irq=proc_irq_save();
    struct proc *p=proc_current();
    int error=-NX_EPERM;
    if(!proc_superuser(p->p_user))goto done;
    error=-NX_EINVAL;
    if(!length || length>NX_HOST_NAME_MAX)goto done;
    error=copyin(&p->p_vm,buffer,name,length);
    if(error)goto done;
    if(!nx_hostname_valid(name,length)){error=-NX_EINVAL;goto done;}
    for(size_t i=0;i<sizeof name;i++)identity.nodename[i]=name[i];
done:
    proc_irq_restore(irq);return error;
}
