#include "signals.h"
#include "fpu.h"
#include "uaccess.h"
#include "segments.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static struct user user;
static struct proc process={.p_user=&user};
static unsigned char stack[8192] __attribute__((aligned(16)));
struct proc *proc_current(void){return &process;}
_Noreturn void proc_exit(int status){(void)status;abort();}
void proc_trace_stop(void){abort();}
bool core_dump(unsigned n,struct interrupt_frame *f){(void)n;(void)f;return false;}
bool vm_translate(const struct address_space *v,uintptr_t a,uintptr_t *p,unsigned *flags)
{(void)v;(void)a;(void)p;*flags=VM_USER|VM_EXEC;return true;}
bool user_access_ok(const struct address_space *v,uintptr_t a,size_t n,bool w)
{(void)v;(void)w;return a>=(uintptr_t)stack && a+n<=(uintptr_t)stack+sizeof(stack);}
int copyout(const struct address_space *v,uintptr_t a,const void *p,size_t n)
{(void)v;memcpy((void *)a,p,n);return 0;}
void fpu_save(struct nx_fpu_state *s)
{memset(s,0,sizeof(*s));__asm__ volatile("fxsave64 %0":"=m"(*s));}
void fpu_restore(const struct nx_fpu_state *s)
{__asm__ volatile("fxrstor64 %0"::"m"(*s));}
void fpu_default(struct nx_fpu_state *s)
{memset(s,0,sizeof(*s));s->bytes[0]=0x7f;s->bytes[1]=3;s->bytes[24]=0x80;s->bytes[25]=0x1f;}
int main(void)
{
    unsigned short control=0x37e,status;
    __asm__ volatile("fninit; fldcw %0; fldz; fldz; fdivp"::"m"(control):"memory");
    __asm__ volatile("fnstsw %0":"=m"(status));assert(status&0x80);
    user.u_signal[NX_SIGFPE]=4096;user.u_restorer[NX_SIGFPE]=8192;
    process.p_sig=1U<<(NX_SIGFPE-1);
    struct interrupt_frame frame={.cs=USER_CS,.ss=USER_DS,.rsp=(uintptr_t)stack+sizeof(stack),.rflags=0x202};
    signal_deliver(&frame);
    __asm__ volatile("fnstsw %0":"=m"(status));assert(!(status&0x80));
    const struct nx_signal_context *saved=(void *)(frame.rsp+8);
    assert(saved->fpu.bytes[2]&1);
    __asm__ volatile("fld1; fstp %%st(0)":::"memory");
    puts("PASS: SIGFPE preserves faulting frame and resets live x87 environment");
}
