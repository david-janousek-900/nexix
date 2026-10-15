/* V7 acct.c sysacct/acct/compress with native record and inode ownership. */
#include "accounting.h"
#include "files.h"
#include "clock.h"
#include <nexix/errno.h>
static struct inode *account_file;
uint16_t accounting_compress(uint64_t ticks)
{
    unsigned exponent=0,round=0;
    while(ticks>=8192){
        if(exponent==7)return UINT16_MAX;
        ++exponent;round=ticks&4;ticks>>=3;
    }
    if(round&&++ticks>=8192){
        if(exponent==7)return UINT16_MAX;
        ticks>>=3;++exponent;
    }
    return (uint16_t)((exponent<<13)+ticks);
}
void accounting_record(const struct user *u,int64_t now,struct nx_acct *record)
{
    *record=(struct nx_acct){0};
    for(unsigned i=0;i<sizeof(record->comm);++i)record->comm[i]=u->u_comm[i];
    record->utime=accounting_compress(u->u_utime<0?0:(uint64_t)u->u_utime);
    record->stime=accounting_compress(u->u_stime<0?0:(uint64_t)u->u_stime);
    record->etime=accounting_compress(now>u->u_start?(uint64_t)now-(uint64_t)u->u_start:0);
    record->btime=u->u_start;record->uid=u->u_ruid;record->gid=u->u_rgid;
    record->tty=u->u_ttyd;record->flag=u->u_acflag;
}
bool proc_superuser(struct user *u)
{if(u->u_uid)return false;u->u_acflag|=NX_ASU;return true;}
int accounting_control(uintptr_t path)
{
    struct user *u=proc_current()->p_user;
    if(!proc_superuser(u))return -NX_EPERM;
    if(!path){
        struct inode *old=account_file;account_file=NULL;
        return old?files_inode_put(old):0;
    }
    if(account_file)return -NX_EBUSY;
    return files_account_open(path,&account_file);
}
void accounting_exit(const struct user *u)
{
    if(!account_file)return;
    struct nx_acct record;accounting_record(u,clock_now(),&record);
    /* V7 never changes the process's exit/wait status on an accounting error. */
    (void)files_account_append(account_file,&record,sizeof(record));
}
