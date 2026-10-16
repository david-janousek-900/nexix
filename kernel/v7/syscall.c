/* Native trap dispatch based on V7 sys/trap.c and sys/sysent.c numbering. */
#include "syscall.h"
#include "clock.h"
#include "accounting.h"
#include "ptrace.h"
#include "profile.h"
#include "kinfo.h"
#include "files.h"
#include "exec.h"
#include "process.h"
#include "signals.h"
#include "tty.h"
#include "segments.h"
#include "uaccess.h"
#include <nexix/syscall.h>
#include <nexix/errno.h>

static int64_t invoke(uint64_t number, struct interrupt_frame *frame)
{
    struct proc *p = proc_current();
    struct user *u = p->p_user;
    switch (number) {
    case NX_SYS_READ: case NX_SYS_WRITE: case NX_SYS_OPEN: case NX_SYS_CLOSE:
    case NX_SYS_CREAT: case NX_SYS_LINK: case NX_SYS_UNLINK: case NX_SYS_CHDIR:
    case NX_SYS_UMOUNT: case NX_SYS_MOUNT: case NX_SYS_MKNOD:
    case NX_SYS_CHROOT:
    case NX_SYS_STAT: case NX_SYS_LSEEK: case NX_SYS_FSTAT: case NX_SYS_DUP:
    case NX_SYS_PIPE: case NX_SYS_DUP2: case NX_SYS_FCNTL:
    case NX_SYS_STTY: case NX_SYS_GTTY: case NX_SYS_IOCTL:
    case NX_SYS_CHMOD: case NX_SYS_CHOWN: case NX_SYS_ACCESS: case NX_SYS_SYNC: case NX_SYS_UTIME:
        return files_syscall(number, u->u_arg);
    case NX_SYS_KINFO:return kinfo_request(u->u_arg[0],u->u_arg[1],u->u_arg[2],u->u_arg[3]);
    case NX_SYS_ACCT: return accounting_control(u->u_arg[0]);
    case NX_SYS_PROFIL:return profile_configure(u->u_arg[0],u->u_arg[1],u->u_arg[2],u->u_arg[3]);
    case NX_SYS_PTRACE:return trace_request((int64_t)u->u_arg[0],(int64_t)u->u_arg[1],
        u->u_arg[2],u->u_arg[3],u->u_arg[4]);
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
    case NX_SYS_SIGNAL: return signal_install(u->u_arg[0], u->u_arg[1], u->u_arg[2]);
    case NX_SYS_KILL: return signal_kill((int64_t)u->u_arg[0], u->u_arg[1]);
    case NX_SYS_BRK: return proc_brk(u->u_arg[0]);
    case NX_SYS_ALARM: {
        uint64_t now = timer_ticks();
        if (u->u_arg[0] > UINT32_MAX) return -NX_EINVAL;
        int64_t old = p->p_clktim > (int64_t)now ? (p->p_clktim - now + NX_CLK_TCK - 1) / NX_CLK_TCK : 0;
        p->p_clktim = u->u_arg[0] ? now + u->u_arg[0] * NX_CLK_TCK : 0;
        return old;
    }
    case NX_SYS_PAUSE:
        while (!proc_sleep_interruptible(u, 40)) {}
        return -NX_EINTR;
    case NX_SYS_TIME: {
        int64_t now = clock_now();
        if (u->u_arg[0]) {
            int error = copyout(&p->p_vm, u->u_arg[0], &now, sizeof(now));
            if (error) return error;
        }
        return now;
    }
    case NX_SYS_STIME: {
        int64_t now;
        if (!proc_superuser(u)) return -NX_EPERM;
        int error = copyin(&p->p_vm, u->u_arg[0], &now, sizeof(now));
        if (error) return error;
        return clock_set(now) ? 0 : -NX_EINVAL;
    }
    case NX_SYS_SETUID: case NX_SYS_SETGID: {
        uintptr_t id = u->u_arg[0];
        if (id > UINT16_MAX) return -NX_EINVAL;
        if (id != (number == NX_SYS_SETUID ? u->u_ruid : u->u_rgid) && !proc_superuser(u)) return -NX_EPERM;
        if (number == NX_SYS_SETUID) p->p_uid = u->u_uid = u->u_ruid = id;
        else u->u_gid = u->u_rgid = id;
        return 0;
    }
    case NX_SYS_NICE: {
        int64_t increment = (int64_t)u->u_arg[0];
        if (increment < -40 || increment > 40) return -NX_EINVAL;
        if (increment < 0 && !proc_superuser(u)) return -NX_EPERM;
        int64_t n = p->p_nice + increment;
        p->p_nice = n < 0 ? 0 : n > 39 ? 39 : n;
        p->p_pri = 50 + ((int)p->p_nice - 20) / 2;
        return 0;
    }
    case NX_SYS_SETPGRP:
        p->p_pgrp = p->p_pid;
        tty_set_group(u->u_ttyp, p->p_pgrp);
        return p->p_pgrp;
    case NX_SYS_GETPGRP: return p->p_pgrp;
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
    uint64_t number = frame->rax;
    int64_t result = number == NX_SYS_SIGRETURN ? signal_return(frame->rdi, frame) : invoke(number, frame);
    if (number == NX_SYS_SIGRETURN && !result) { u->u_error = 0; return; }
    u->u_error = result < 0 ? -result : 0;
    u->u_r.r_val1 = result;
    frame->rax = (uint64_t)result;
    /* Normal returns preserve the saved frame; successful exec installs a new
     * entry frame and returns directly to its entry point via IRETQ. */
}
