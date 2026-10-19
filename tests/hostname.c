#include "utsname.h"
#include "process.h"
#include "uaccess.h"
#include <nexix/hostname.h>
#include <nexix/errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct proc process;
static struct user user;
static struct nx_utsname snapshot;
static int locked,fail_input,fail_output;
uintptr_t proc_irq_save(void){assert(!locked);locked=1;return 0x200;}
void proc_irq_restore(uintptr_t f){assert(locked && f==0x200);locked=0;}
struct proc *proc_current(void){return &process;}
bool proc_superuser(struct user *u){assert(locked);return u->u_uid==0;}
int copyin(const struct address_space *vm,uintptr_t p,void *to,size_t n)
{assert(locked && vm==&process.p_vm);if(fail_input || !p)return -NX_EFAULT;memcpy(to,(void *)p,n);return 0;}
int copyout(const struct address_space *vm,uintptr_t p,const void *from,size_t n)
{assert(locked && vm==&process.p_vm && n==sizeof snapshot);if(fail_output || !p)return -NX_EFAULT;memcpy((void *)p,from,n);return 0;}
static void check(const char *name)
{assert(uname_request((uintptr_t)&snapshot)==0 && !locked);assert(!strcmp(snapshot.nodename,name));assert(!strcmp(snapshot.sysname,"Nexix"));}
int main(void)
{
    process.p_user=&user;check("nexix");
    const char name[]="node.example.test";
    assert(hostname_request((uintptr_t)name,strlen(name))==0);check(name);
    user.u_uid=100;assert(hostname_request(0,0)==-NX_EPERM);check(name);user.u_uid=0;
    const char *bad[]={"","a..b",".host","host.","-a","a-","a.-b","a_.b","a b","a\nb","a;b","a$b"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++){assert(hostname_request((uintptr_t)bad[i],strlen(bad[i]))==-NX_EINVAL);check(name);}
    assert(hostname_request((uintptr_t)"a\0b",3)==-NX_EINVAL);check(name);
    assert(hostname_request((uintptr_t)name,65)==-NX_EINVAL);check(name);
    fail_input=1;assert(hostname_request((uintptr_t)name,5)==-NX_EFAULT);fail_input=0;check(name);
    fail_output=1;assert(uname_request(1)==-NX_EFAULT);fail_output=0;check(name);
    char maximum[65];memset(maximum,'a',64);maximum[31]='.';maximum[64]=0;
    assert(hostname_request((uintptr_t)maximum,64)==0);check(maximum);
    assert(hostname_request((uintptr_t)"short",5)==0);check("short");
    for(size_t i=6;i<sizeof snapshot.nodename;i++)assert(snapshot.nodename[i]==0);
    puts("PASS: kernel hostname permissions, validation, atomic failures, bounds, zeroing and uname snapshots");
}
