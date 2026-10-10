/* Native trap dispatch based on V7 sys/trap.c and sys/sysent.c numbering. */
#include "syscall.h"
#include "exec.h"
#include "process.h"
#include "segments.h"
#include "uaccess.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>

static int64_t invoke(uint64_t number, struct interrupt_frame *frame)
{
    struct proc *p = proc_current();
    struct user *u = p->p_user;
    switch (number) {
    case NX_SYS_EXIT:
        proc_exit((u->u_arg[0] & 255) << 8);
    case NX_SYS_EXEC:
        return proc_exec(u->u_arg[0], u->u_arg[1], 0, frame);
    case NX_SYS_EXECVE:
        return proc_exec(u->u_arg[0], u->u_arg[1], u->u_arg[2], frame);
    case NX_SYS_FORK:
        return proc_fork(frame);
    case NX_SYS_WAIT: {
        uintptr_t address = u->u_arg[0];
        /* Validate before wait can block or consume a zombie. Private mappings
         * cannot change while this process sleeps: no threads/remote VM edits. */
        if (address && !user_access_ok(&p->p_vm, address, sizeof(int32_t), true))
            return -NX_EFAULT;
        int32_t status = 0;
        v7_pid_t pid = proc_wait(&status);
        if (pid < 0) return pid;
        if (address && copyout(&p->p_vm, address, &status, sizeof(status)))
            panic("prevalidated wait status mapping changed");
        return pid;
    }
    case NX_SYS_GETPID: return p->p_pid;
    case NX_SYS_GETPPID: return p->p_ppid;
    case NX_SYS_GETUID: return u->u_ruid;
    case NX_SYS_GETEUID: return u->u_uid;
    case NX_SYS_GETGID: return u->u_rgid;
    case NX_SYS_GETEGID: return u->u_gid;
    case NX_SYS_TIMES: {
        struct nx_tms times = {u->u_utime, u->u_stime, u->u_cutime, u->u_cstime};
        int error = copyout(&p->p_vm, u->u_arg[0], &times, sizeof(times));
        return error ? error : (int64_t)timer_ticks();
    }
    case NX_SYS_UMASK: {
        uint16_t old = u->u_cmask;
        u->u_cmask = u->u_arg[0] & 0777;
        return old;
    }
    case NX_SYS_YIELD: proc_yield(); return 0;
    default: return -NX_ENOSYS;
    }
}
void syscall_dispatch(struct interrupt_frame *frame)
{
    struct proc *p = proc_current();
    if (!p || frame->cs != USER_CS || frame->ss != USER_DS || !interrupt_stack_valid(frame))
        panic("syscall outside a user process");
    struct user *u = p->p_user;
    u->u_error = 0;
    u->u_arg[0] = frame->rdi;
    u->u_arg[1] = frame->rsi;
    u->u_arg[2] = frame->rdx;
    u->u_arg[3] = frame->r10;
    u->u_arg[4] = frame->r8;
    u->u_arg[5] = frame->r9;
    u->u_ap = u->u_arg;
    u->u_ar0 = frame;
    u->u_r.r_val1 = u->u_r.r_val2 = 0;
    int64_t result = invoke(frame->rax, frame);
    u->u_error = result < 0 ? -result : 0;
    u->u_r.r_val1 = result;
    frame->rax = (uint64_t)result;
    /* Normal returns preserve the saved frame; successful exec installs a new
     * entry frame and returns directly to its entry point via IRETQ. */
}
