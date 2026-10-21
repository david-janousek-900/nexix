/* Native delivery of V7 sig.c/sys4.c signals. No masks or job control. */
#include "signals.h"
#include "fpu.h"
#include "uaccess.h"
#include "segments.h"
#include "core.h"
#include "ptrace.h"
#include <nexix/errno.h>
_Static_assert(sizeof(struct interrupt_frame) == offsetof(struct nx_signal_context, fpu), "signal context prefix");
_Static_assert(offsetof(struct interrupt_frame, rsp) == offsetof(struct nx_signal_context, rsp), "signal context offsets");
static bool code(struct proc *p, uintptr_t address)
{
    unsigned permissions;
    return vm_translate(&p->p_vm, address, NULL, &permissions) &&
        (permissions & (VM_USER | VM_EXEC)) == (VM_USER | VM_EXEC);
}
bool signal_pending(struct proc *p)
{
    for (unsigned n = 1; n < V7_NSIG; ++n) {
        unsigned bit = 1U << (n - 1);
        if (!(p->p_sig & bit)) continue;
        if (!(p->p_flag&V7_STRC) && n != NX_SIGKILL && p->p_user->u_signal[n] == 1) p->p_sig &= ~bit;
        else return true;
    }
    return false;
}
void signal_post(struct proc *p, unsigned n)
{
    if (!n || n >= V7_NSIG || p->p_stat == V7_UNUSED ||
        p->p_stat == V7_SIDL || p->p_stat == V7_SZOMB || (p->p_flag & V7_SSYS)) return;
    if (!(p->p_flag&V7_STRC) && n != NX_SIGKILL && p->p_user->u_signal[n] == 1) return;
    p->p_sig |= 1U << (n - 1);
    if (p->p_stat == V7_SSLEEP && p->p_interruptible) {
        p->p_wchan = NULL; p->p_timed_sleep = false; p->p_stat = V7_SRUN;
    }
    /* The tracer already saw this stop; continuing must not discard a later SIGKILL. */
    if (n == NX_SIGKILL && p->p_stat == V7_SSTOP && (p->p_flag & V7_STRC)) p->p_trace_kill = true;
}
void signal_group(v7_pid_t group, unsigned n)
{
    if (!group) return;
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *p = (struct proc *)&proc_table()[i];
        if (p->p_pgrp == group) signal_post(p, n);
    }
}
int64_t signal_install(uint64_t n, uintptr_t handler, uintptr_t restorer)
{
    struct proc *p = proc_current();
    if (!n || n >= V7_NSIG || n == NX_SIGKILL) return -NX_EINVAL;
    if (handler > 1 && (!code(p, handler) || !code(p, restorer))) return -NX_EFAULT;
    uintptr_t old = p->p_user->u_signal[n];
    p->p_user->u_signal[n] = handler;
    p->p_user->u_restorer[n] = restorer;
    p->p_sig &= ~(1U << (n - 1)); /* Historical signal() clears pending bit. */
    return old;
}
static int kill_request(int64_t pid, uint64_t n, bool group_only)
{
    if (pid < INT32_MIN || pid > INT32_MAX || n >= V7_NSIG) return -NX_EINVAL;
    struct proc *caller = proc_current();
    bool found = false, permitted = false;
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *p = (struct proc *)&proc_table()[i];
        if (p->p_stat == V7_UNUSED || p->p_stat == V7_SIDL || p->p_stat == V7_SZOMB || (p->p_flag & V7_SSYS)) continue;
        bool match = group_only ? p->p_pgrp == -pid : pid > 0 ? p->p_pid == pid : pid == 0 ? p->p_pgrp == caller->p_pgrp :
            pid == -1 ? p->p_pid != 1 && p != caller : p->p_pgrp == -pid;
        if (!match) continue;
        found = true;
        if (caller->p_user->u_uid && caller->p_user->u_uid != p->p_uid) continue;
        permitted = true; signal_post(p, n);
    }
    return permitted ? 0 : found ? -NX_EPERM : -NX_ESRCH;
}
int signal_kill(int64_t pid,uint64_t n){return kill_request(pid,n,false);}
int signal_group_user(v7_pid_t group,unsigned n)
{
    if(group<=0)return -NX_EINVAL;
    return kill_request(-(int64_t)group,n,true);
}
void signal_deliver(struct interrupt_frame *frame)
{
    struct proc *p = proc_current();
    if (!p || (frame->cs & 3) != 3 || !signal_pending(p)) return;
    if(p->p_flag&V7_STRC){
        proc_trace_stop();
        /* Continue replaces the pending set, including suppression with 0.
         * Deliver the selected replacement once without stopping again. */
        if(!p->p_sig)return;
    }
    unsigned n;
    for (n = 1; n < V7_NSIG; ++n) if (p->p_sig & (1U << (n - 1))) break;
    p->p_sig &= ~(1U << (n - 1));
    uintptr_t handler = p->p_user->u_signal[n], restorer = p->p_user->u_restorer[n];
    if(handler==1&&n!=NX_SIGKILL)return;
    if (!handler || n == NX_SIGKILL) proc_exit(n|(core_dump(n,frame)?0200:0));
    /* Preserve the SysV red zone and align the handler as a normal call.
     * Saved frames reside in user memory, permitting handler longjmp without
     * retaining stale kernel frames. sigreturn validates every privilege field. */
    size_t size = sizeof(struct nx_signal_context);
    struct nx_fpu_state fp;
    fpu_save(&fp);
    if (frame->rsp < USER_BASE + 128 + size + 24) proc_exit(NX_SIGSEGV|(core_dump(NX_SIGSEGV,frame)?0200:0));
    uintptr_t stack = ((frame->rsp - 128 - size) & ~15UL) - 8;
    if (!code(p, handler) || !code(p, restorer) ||
        !user_access_ok(&p->p_vm, stack, size + 8, true) ||
        copyout(&p->p_vm, stack, &restorer, 8) ||
        copyout(&p->p_vm, stack + 8, frame, sizeof(*frame)) ||
        copyout(&p->p_vm, stack + 8 + sizeof(*frame), &fp, sizeof(fp))) proc_exit(NX_SIGSEGV|(core_dump(NX_SIGSEGV,frame)?0200:0));
    /* The saved frame retains the faulting environment for sigreturn. Give
     * handlers a clean live environment, including masked x87/SSE exceptions. */
    struct nx_fpu_state clean;
    fpu_default(&clean);fpu_restore(&clean);
    if (n != NX_SIGILL && n != NX_SIGTRAP) p->p_user->u_signal[n] = 0;
    frame->rip = handler; frame->rsp = stack; frame->rdi = n;
    frame->rflags &= ~0x500UL; /* SysV requires DF clear; do not single-step entry. */
}
int signal_return(uintptr_t address, struct interrupt_frame *frame)
{
    struct proc *p = proc_current();
    struct interrupt_frame saved;
    struct nx_fpu_state fp;
    int error = copyin(&p->p_vm, address, &saved, sizeof(saved));
    if (error) return error;
    /* Only ordinary user arithmetic, direction, trace and resume flags. */
    if (saved.cs != USER_CS || saved.ss != USER_DS ||
        (saved.rflags & ~0x250fd7UL) || (saved.rflags & 0x202) != 0x202 ||
        !code(p, saved.rip) || !saved.rsp ||
        !user_access_ok(&p->p_vm, saved.rsp - 1, 1, true)) return -NX_EINVAL;
    if (address > UINTPTR_MAX - sizeof(saved)) return -NX_EFAULT;
    error = copyin(&p->p_vm, address + sizeof(saved), &fp, sizeof(fp));
    if (error) return error;
    if (!fpu_valid(&fp)) return -NX_EINVAL;
    saved.vector = saved.error = 0;
    *frame = saved;
    fpu_restore(&fp);
    return 0;
}
