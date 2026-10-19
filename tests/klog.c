#include "klog.h"
#include "process.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct proc process;
static struct user user;
static char output[64];
static int locked,writes,denied;
static const char text[]="[    0.000000] boot\n";
uintptr_t proc_irq_save(void){assert(!locked);locked=1;return 123;}
void proc_irq_restore(uintptr_t f){assert(locked && f==123);locked=0;}
struct proc *proc_current(void){return &process;}
bool proc_superuser(struct user *u){assert(locked);return !u->u_uid;}
size_t console_log_copy(char *p,size_t n)
{assert(locked && n>=sizeof(text)-1);memcpy(p,text,sizeof(text)-1);return sizeof(text)-1;}
int copyout(const struct address_space *vm,uintptr_t p,const void *source,size_t n)
{
    assert(locked && vm==&process.p_vm);
    if(denied || p!=(uintptr_t)output || n>sizeof(output))return -NX_EFAULT;
    memcpy(output,source,n);++writes;return 0;
}
int main(void)
{
    assert(klog_request(0,0)==-NX_EPERM);
    process.p_user=&user;user.u_uid=100;
    assert(klog_request(0,0)==-NX_EPERM && !writes);
    user.u_uid=0;assert(klog_request(0,0)==sizeof(text)-1 && !writes);
    memset(output,0xa5,sizeof(output));
    assert(klog_request((uintptr_t)output,1)==-NX_ERANGE && !writes && output[0]==(char)0xa5);
    assert(klog_request(0,sizeof(output))==-NX_EFAULT);
    denied=1;assert(klog_request((uintptr_t)output,sizeof(output))==-NX_EFAULT && !writes);
    denied=0;assert(klog_request((uintptr_t)output,sizeof(output))==sizeof(text)-1);
    assert(writes==1 && !memcmp(output,text,sizeof(text)-1) && output[sizeof(text)-1]==(char)0xa5);
    assert(klog_request((uintptr_t)output,sizeof(output))==sizeof(text)-1 && writes==2);
    assert(!locked);
    puts("PASS: kernel-log authorization, sizing, short/invalid buffers and non-destructive snapshots");
}
