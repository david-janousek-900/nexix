/* V7 sig.c ptrace/procxmt adapted to resident private x86_64 address spaces.
 * Single CPU, IRQs disabled: parent can access the stopped child's VM/frame
 * directly without the PDP-11 global IPC rendezvous and temporary rescheduling. */
#include "ptrace.h"
#include "uaccess.h"
#include "fpu.h"
#include <nexix/ptrace.h>
#include <nexix/errno.h>
_Static_assert(sizeof(struct interrupt_frame)==NX_PT_OFFSET(fpu),"trace frame prefix");
_Static_assert(offsetof(struct interrupt_frame,rip)==NX_PT_OFFSET(rip),"trace PC offset");
_Static_assert(offsetof(struct interrupt_frame,rsp)==NX_PT_OFFSET(rsp),"trace stack offset");
_Static_assert(offsetof(struct interrupt_frame,rflags)==NX_PT_OFFSET(rflags),"trace flags offset");
static bool executable(struct proc *p,uintptr_t address)
{
    unsigned flags;
    return vm_translate(&p->p_vm,address,NULL,&flags)&&
        (flags&(VM_USER|VM_EXEC))==(VM_USER|VM_EXEC);
}
static int user_word(struct proc *p,uintptr_t address,uint64_t *word,bool write)
{
    if(address%8||address>sizeof(struct nx_ptrace_user)-8)return -NX_EIO;
    struct interrupt_frame *frame=p->p_user->u_ar0;
    if(address<NX_PT_OFFSET(fpu)){
        uint64_t *slot=(uint64_t *)((uint8_t *)frame+address);
        if(!write){*word=*slot;return 0;}
        if(address>=NX_PT_OFFSET(vector)&&address!=NX_PT_OFFSET(rip)&&
           address!=NX_PT_OFFSET(rsp)&&address!=NX_PT_OFFSET(rflags))return -NX_EIO;
        if(address==NX_PT_OFFSET(rip)&&!executable(p,*word))return -NX_EIO;
        if(address==NX_PT_OFFSET(rsp)&&(!*word||!user_access_ok(&p->p_vm,*word-1,1,true)))return -NX_EIO;
        /* Arithmetic/DF/RF/AC/ID only, IF and bit 1 forced, no TF until task 58.
         * Never allow IOPL, NT, VM, reserved bits or kernel selectors. */
        if(address==NX_PT_OFFSET(rflags))*word=(*word&0x250cd5UL)|0x202;
        *slot=*word;return 0;
    }
    struct nx_fpu_state fp=p->p_fpu;
    uint8_t *bytes=fp.bytes+address-NX_PT_OFFSET(fpu);
    if(!write){
        *word=0;for(unsigned i=0;i<8;++i)*word|=(uint64_t)bytes[i]<<(8*i);
    }else{
        for(unsigned i=0;i<8;++i)bytes[i]=(uint8_t)(*word>>(8*i));
        if(!fpu_valid(&fp))return -NX_EIO;
        p->p_fpu=fp;
    }
    return 0;
}
int trace_request(int64_t request,int64_t pid,uintptr_t address,uint64_t data,uintptr_t result)
{
    struct proc *caller=proc_current(),*child=NULL;
    if(request<=0){caller->p_flag|=V7_STRC;return 0;}
    for(unsigned i=1;i<V7_NPROC;++i){
        struct proc *p=(struct proc *)&proc_table()[i];
        if(p->p_stat==V7_SSTOP&&p->p_pid==pid&&p->p_ppid==caller->p_pid){child=p;break;}
    }
    if(!child)return -NX_ESRCH;
    /* A V7 examine request restops the child and permits another wait report. */
    child->p_flag&=~V7_SWTED;
    uint64_t word=data;int error;
    switch(request){
    case NX_PT_READ_I:case NX_PT_READ_D:case NX_PT_READ_U:
        if(!user_access_ok(&caller->p_vm,result,8,true))return -NX_EFAULT;
        if(request==NX_PT_READ_U)error=user_word(child,address,&word,false);
        else error=(address%8||copyin(&child->p_vm,address,&word,8))?-NX_EIO:0;
        if(error)return error;
        return copyout(&caller->p_vm,result,&word,8);
    case NX_PT_WRITE_U:return user_word(child,address,&word,true);
    case NX_PT_CONTINUE:
        if(data>=V7_NSIG)return -NX_EIO;
        if(address!=1&&!executable(child,address))return -NX_EIO;
        if(address!=1)child->p_user->u_ar0->rip=address;
        child->p_sig=data?1U<<(data-1):0;
        child->p_stat=V7_SRUN;return 0;
    case NX_PT_KILL:
        child->p_trace_exit=true;child->p_stat=V7_SRUN;return 0;
    default:return -NX_EIO; /* Memory writes and step: task 58. */
    }
}
