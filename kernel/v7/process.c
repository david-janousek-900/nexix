/* Native adaptation of process behavior from V7 sys/{slp,sys1}.c.
 * Original sources and Caldera license are retained under reference/. */
#include "process.h"
#include "phys.h"
#include "fpu.h"
#include "clock.h"
#include "files.h"
#include "accounting.h"
#include "text.h"
#include "signals.h"
#include "ptrace.h"
#include "profile.h"
#include "exec.h"
#include "segments.h"
#include "serial.h"
#include <limits.h>

struct process_stack { uint8_t guard[PAGE_SIZE], bytes[ENTRY_STACK_SIZE]; };
static struct process_stack stacks[V7_NPROC] __attribute__((aligned(PAGE_SIZE)));
static struct proc processes[V7_NPROC];
static struct user users[V7_NPROC];
static struct proc *current, *init_process;
static struct v7_context scheduler;
static unsigned cursor;
static v7_pid_t next_pid;
static int allocation_error;
static bool initialized, scheduling;
static char timer_channel;
extern void arch_context_switch(struct v7_context *, const struct v7_context *);
extern _Noreturn void arch_process_enter(struct interrupt_frame *);

uintptr_t proc_irq_save(void)
{
    uintptr_t flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
void proc_irq_restore(uintptr_t flags)
{
    if (flags & 0x200) __asm__ volatile ("sti" : : : "memory");
}
static void require_locked(void)
{
    uintptr_t flags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(flags));
    if (flags & 0x200) panic("process operation requires interrupts disabled");
}
static void clear(void *pointer, size_t size)
{
    uint8_t *bytes = pointer;
    for (size_t i = 0; i < size; ++i) bytes[i] = 0;
}
static void copy(void *destination, const void *source, size_t size)
{
    uint8_t *d = destination;
    const uint8_t *s = source;
    for (size_t i = 0; i < size; ++i) d[i] = s[i];
}
struct proc *proc_current(void) { return current; }
const struct proc *proc_table(void) { return processes; }
static bool member(const struct proc *p)
{
    for (unsigned i = 1; i < V7_NPROC; ++i)
        if (p == &processes[i]) return true;
    return false;
}
static struct proc *find_pid(v7_pid_t pid)
{
    for (unsigned i = 0; i < V7_NPROC; ++i)
        if (processes[i].p_stat != V7_UNUSED && processes[i].p_pid == pid)
            return &processes[i];
    return NULL;
}
static int failure(int error)
{
    if (current) current->p_user->u_error = error;
    return -error;
}
static v7_pid_t allocate_pid(void)
{
    /* Preserve V7's PID ceiling; avoid both active PIDs and process groups. */
    for (;;) {
        if (++next_pid >= 30000) next_pid = 1;
        bool used = false;
        for (unsigned i = 0; i < V7_NPROC; ++i)
            if (processes[i].p_stat != V7_UNUSED &&
                (processes[i].p_pid == next_pid || processes[i].p_pgrp == next_pid))
                used = true;
        if (!used) return next_pid;
    }
}

void proc_init(void)
{
    require_locked();
    if (initialized) panic("process table initialized twice");
    for (unsigned i = 1; i < V7_NPROC; ++i)
        if (!paging_guard_page((uintptr_t)stacks[i].guard))
            panic("cannot guard process stack");
    processes[0].p_stat = V7_SIDL; /* Bootstrap scheduler and fallback reaper. */
    processes[0].p_flag = V7_SSYS | V7_SLOAD;
    processes[0].p_user = &users[0];
    users[0].u_procp = &processes[0];
    initialized = true;
}

struct proc *proc_alloc(void)
{
    require_locked();
    if (!initialized) panic("process table is not initialized");
    allocation_error = 0;
    unsigned owned = 0;
    if (current && current->p_uid)
        for (unsigned i = 1; i < V7_NPROC; ++i)
            if (processes[i].p_stat != V7_UNUSED && processes[i].p_uid == current->p_uid)
                ++owned;
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *p = &processes[i];
        if (p->p_stat != V7_UNUSED) continue;
        if (current && current->p_uid && (owned >= 25 || i == V7_NPROC - 1)) {
            allocation_error = V7_EAGAIN;
            failure(allocation_error);
            return NULL;
        }
        clear(p, sizeof(*p));
        fpu_default(&p->p_fpu);
        clear(&users[i], sizeof(users[i]));
        clear(stacks[i].bytes, sizeof(stacks[i].bytes));
        p->p_stat = V7_SIDL;
        p->p_pid = allocate_pid();
        p->p_ppid = current ? current->p_pid : 0;
        p->p_uid = current ? current->p_uid : 0;
        p->p_pgrp = current ? current->p_pgrp : p->p_pid;
        p->p_flag = V7_SLOAD;
        p->p_pri = 50;
        p->p_nice = current ? current->p_nice : 20;
        p->p_stack_bottom = (uintptr_t)stacks[i].bytes;
        p->p_stack_top = p->p_stack_bottom + ENTRY_STACK_SIZE;
        p->p_user = &users[i];
        p->p_user->u_start = clock_now();
        if (current) {
            users[i].u_uid = current->p_user->u_uid;
            users[i].u_gid = current->p_user->u_gid;
            users[i].u_ruid = current->p_user->u_ruid;
            users[i].u_rgid = current->p_user->u_rgid;
        }
        users[i].u_procp = p;
        users[i].u_ap = users[i].u_arg;
        users[i].u_ttyd = V7_NODEV;
        if (!vm_create(&p->p_vm)) {
            clear(p, sizeof(*p));
            clear(&users[i], sizeof(users[i]));
            allocation_error = V7_ENOMEM;
            failure(allocation_error);
            return NULL;
        }
        return p;
    }
    allocation_error = V7_EAGAIN;
    failure(allocation_error);
    return NULL;
}

static bool free_mapping(uintptr_t va, uintptr_t pa, unsigned permissions, void *argument)
{
    (void)va; (void)argument;
    if(permissions&VM_PHYS){phys_release(pa);return true;}
    if (!page_free(pa)) panic("invalid process-owned page");
    return true;
}
static void release_vm(struct proc *p)
{
    if (!p->p_vm.root) return;
    /* Private leaves own frames; physical leaves own manager references. */
    if (!vm_walk_user(&p->p_vm, free_mapping, NULL) || !vm_destroy(&p->p_vm))
        panic("cannot release process address space");
    p->p_size = 0;
}
void proc_abort(struct proc *p)
{
    require_locked();
    if (!member(p) || p == current || p->p_stat != V7_SIDL)
        panic("invalid unpublished process abort");
    release_vm(p);
    clear(p->p_user, sizeof(*p->p_user));
    clear(p, sizeof(*p));
}
bool proc_map_page(struct proc *p, uintptr_t address, unsigned permissions, const void *contents)
{
    require_locked();
    if (!member(p) || p->p_stat != V7_SIDL) return false;
    uintptr_t physical = page_alloc();
    if (!physical) return false;
    if (contents) copy((void *)physical, contents, PAGE_SIZE);
    else clear((void *)physical, PAGE_SIZE);
    if (!vm_map(&p->p_vm, address, physical, permissions)) {
        if (!page_free(physical)) panic("cannot roll back process mapping");
        return false;
    }
    p->p_size += PAGE_SIZE;
    return true;
}

static _Noreturn void bootstrap(void)
{
    if (!current || current->p_stat != V7_SRUN) panic("invalid process bootstrap");
    if (!(current->p_flag & V7_SSYS)) arch_process_enter(current->p_user->u_ar0);
    __asm__ volatile ("sti" : : : "memory");
    current->p_entry(current->p_argument);
    proc_exit(0);
}
static void prepare_context(struct proc *p, struct interrupt_frame *frame)
{
    uintptr_t top = p->p_stack_top;
    if (frame) {
        top -= sizeof(*frame);
        copy((void *)top, frame, sizeof(*frame));
        p->p_user->u_ar0 = (struct interrupt_frame *)top;
    }
    p->p_context.rsp = top - 8; /* SysV entry alignment: RSP mod 16 == 8. */
    *(uint64_t *)p->p_context.rsp = 0;
    p->p_context.rip = (uintptr_t)bootstrap;
}
bool proc_start_user(struct proc *p, uintptr_t entry, uintptr_t stack)
{
    require_locked();
    unsigned code_permissions, stack_permissions;
    if (!member(p) || p->p_stat != V7_SIDL || !stack || (stack & 15) ||
        !vm_translate(&p->p_vm, entry, NULL, &code_permissions) ||
        (code_permissions & (VM_USER | VM_EXEC)) != (VM_USER | VM_EXEC) ||
        !vm_translate(&p->p_vm, stack - 1, NULL, &stack_permissions) ||
        (stack_permissions & (VM_USER | VM_WRITE)) != (VM_USER | VM_WRITE))
        return false;
    struct interrupt_frame frame = { .rip = entry, .rsp = stack,
        .cs = USER_CS, .ss = USER_DS, .rflags = 0x202 };
    prepare_context(p, &frame);
    p->p_stat = V7_SRUN;
    return true;
}
bool proc_start_init(struct proc *p, uintptr_t entry, uintptr_t stack)
{
    require_locked();
    if (init_process || !member(p) || p->p_pid != 1 || p->p_ppid ||
        !proc_start_user(p, entry, stack)) return false;
    init_process = p;
    return true;
}
v7_pid_t proc_spawn(void (*entry)(void *), void *argument)
{
    uintptr_t flags = proc_irq_save();
    if (!entry) {
        int result = failure(V7_EINVAL);
        proc_irq_restore(flags);
        return result;
    }
    struct proc *p = proc_alloc();
    if (!p) {
        int error = allocation_error;
        proc_irq_restore(flags);
        return -error;
    }
    p->p_flag |= V7_SSYS;
    p->p_entry = entry;
    p->p_argument = argument;
    prepare_context(p, NULL);
    p->p_stat = V7_SRUN;
    v7_pid_t pid = p->p_pid;
    proc_irq_restore(flags);
    return pid;
}

static void switch_out(void)
{
    require_locked();
    if (!current || !scheduling) panic("switch without process scheduler");
    fpu_save(&current->p_fpu);
    arch_context_switch(&current->p_context, &scheduler);
}
void proc_trace_stop(void)
{
    require_locked();
    struct proc *p=current,*parent=find_pid(p->p_ppid);
    /* V7 cannot leave an orphan stopped under init. Bootstrap PID 0 is a
     * valid test reaper, but cannot act as a user debugger. */
    if(!parent||p->p_ppid<=1||parent->p_stat==V7_SZOMB)goto orphan;
    v7_pid_t debugger=p->p_ppid;
    p->p_stat=V7_SSTOP;p->p_flag&=~V7_SWTED;
    proc_wakeup(parent);
    switch_out();
    if(p->p_trace_exit||p->p_ppid!=debugger)goto orphan;
    return;
orphan:
    for(unsigned n=1;n<V7_NSIG;++n)if(p->p_sig&(1U<<(n-1)))proc_exit(n);
    proc_exit(0);
}
void proc_yield(void)
{
    uintptr_t flags = proc_irq_save();
    if (!current || current->p_stat != V7_SRUN) panic("yield from invalid process");
    switch_out();
    proc_irq_restore(flags);
}
void proc_sleep(void *channel, unsigned priority)
{
    uintptr_t flags = proc_irq_save();
    if (!current || !channel || priority > 127) panic("invalid process sleep");
    unsigned previous_priority = current->p_pri;
    current->p_wchan = channel;
    current->p_pri = priority;
    current->p_stat = V7_SSLEEP;
    switch_out();
    current->p_pri = previous_priority;
    proc_irq_restore(flags);
}
int proc_sleep_interruptible(void *channel, unsigned priority)
{
    uintptr_t flags = proc_irq_save();
    if (signal_pending(current)) { proc_irq_restore(flags); return -V7_EINTR; }
    current->p_interruptible = true;
    proc_sleep(channel, priority);
    current->p_interruptible = false;
    int error = signal_pending(current) ? -V7_EINTR : 0;
    proc_irq_restore(flags);
    return error;
}
unsigned proc_wakeup(void *channel)
{
    uintptr_t flags = proc_irq_save();
    unsigned count = 0;
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *p = &processes[i];
        if (p->p_stat == V7_SSLEEP && p->p_wchan == channel) {
            p->p_wchan = NULL;
            p->p_timed_sleep = false;
            p->p_stat = V7_SRUN;
            ++count;
        }
    }
    proc_irq_restore(flags);
    return count;
}
void proc_delay(uint64_t ticks)
{
    if (!ticks) { proc_yield(); return; }
    if (ticks > INT64_MAX) panic("process delay too long");
    uintptr_t flags = proc_irq_save();
    if (!current) panic("delay without process");
    current->p_deadline = timer_ticks() + ticks;
    current->p_timed_sleep = true;
    proc_sleep(&timer_channel, 40);
    proc_irq_restore(flags);
}

void proc_run(void)
{
    require_locked();
    if (!initialized || scheduling || current) panic("invalid scheduler entry");
    scheduling = true;
    for (;;) {
        struct proc *selected = NULL;
        bool live = false;
        for (unsigned n = 0; n < V7_NPROC - 1; ++n) {
            unsigned i = 1 + (cursor + n) % (V7_NPROC - 1);
            struct proc *p = &processes[i];
            if (p->p_stat == V7_SRUN || p->p_stat == V7_SSLEEP || p->p_stat == V7_SSTOP)
                live = true;
            if (p->p_stat == V7_SRUN && (!selected || p->p_pri < selected->p_pri))
                selected = p;
        }
        if (!live) break;
        if (!selected) {
            /* IF stays clear from the scan through HLT: no lost IRQ wakeup. */
            __asm__ volatile ("sti; hlt; cli" : : : "memory");
            continue;
        }
        cursor = (unsigned)(selected - processes) % (V7_NPROC - 1);
        current = selected;
        current->p_slice = 0;
        vm_activate(&current->p_vm);
        kernel_stack_set(current->p_stack_bottom, current->p_stack_top);
        fpu_restore(&current->p_fpu);
        arch_context_switch(&scheduler, &current->p_context);
        struct proc *previous = current;
        current = NULL;
        vm_activate(paging_kernel_space());
        kernel_stack_set(entry_stack_bottom(0), entry_stack_top(0));
        /* Never free pages or recycle a slot on the exiting process's stack. */
        if (previous->p_stat == V7_SZOMB) release_vm(previous);
    }
    scheduling = false;
}

static bool clone_mapping(uintptr_t va, uintptr_t pa, unsigned permissions, void *argument)
{
    struct proc *p=argument;
    if(permissions&VM_PHYS){
        if(!phys_clone_leaf(&p->p_vm,va,pa))return false;
        p->p_size+=PAGE_SIZE;return true;
    }
    return proc_map_page(p, va, permissions & (VM_WRITE | VM_EXEC), (const void *)pa);
}
v7_pid_t proc_fork(const struct interrupt_frame *frame)
{
    uintptr_t flags = proc_irq_save();
    if (!current || (current->p_flag & V7_SSYS) || !frame ||
        frame->cs != USER_CS || frame->ss != USER_DS || !interrupt_stack_valid(frame)) {
        int result = failure(V7_EINVAL);
        proc_irq_restore(flags);
        return result;
    }
    struct user *u = current->p_user;
    bool referenced = (current->p_textp&&!text_valid(current->p_textp)) || !files_refs_valid(u);
    if (referenced) {
        int result = failure(V7_EBUSY);
        proc_irq_restore(flags);
        return result;
    }
    struct proc *child = proc_alloc();
    if (!child) { int result = -u->u_error; proc_irq_restore(flags); return result; }
    if (!vm_walk_user(&current->p_vm, clone_mapping, child)) {
        proc_abort(child);
        int result = failure(V7_ENOMEM);
        proc_irq_restore(flags);
        return result;
    }
    copy(child->p_user, u, sizeof(*u));
    fpu_save(&child->p_fpu);
    files_fork(child->p_user);
    child->p_textp=current->p_textp;if(child->p_textp)text_retain(child->p_textp);
    child->p_user->u_procp = child;
    child->p_user->u_ap = child->p_user->u_arg;
    child->p_user->u_error = 0;
    child->p_user->u_utime = child->p_user->u_stime = 0;
    child->p_user->u_cutime = child->p_user->u_cstime = 0;
    child->p_user->u_start = clock_now();
    child->p_user->u_acflag = NX_AFORK;
    struct interrupt_frame child_frame = *frame;
    child_frame.rax = 0;
    /* Tracing, including a debugger's in-flight step, is per process. The
     * untraced child must not inherit a step over the parent's fork syscall. */
    if(current->p_flag&V7_STRC)child_frame.rflags&=~0x100UL;
    prepare_context(child, &child_frame);
    child->p_brk_base = current->p_brk_base;
    child->p_brk = current->p_brk;
    child->p_pri = current->p_pri;
    child->p_stat = V7_SRUN; /* Publish only after the entire copy succeeds. */
    v7_pid_t pid = child->p_pid;
    proc_irq_restore(flags);
    return pid;
}

_Noreturn void proc_exit(int32_t status)
{
    (void)proc_irq_save();
    if (!current) panic("exit without process");
    if (current == init_process) panic("init exited");
    struct proc *p = current;
    /* files_exit drops TTY references; preserve its number for V7 acct. */
    v7_dev_t tty=p->p_user->u_ttyd;
    files_exit(p->p_user);
    p->p_user->u_ttyd=tty;accounting_exit(p->p_user);p->p_user->u_ttyd=V7_NODEV;
    p->p_xstat = status;
    p->p_utime = p->p_user->u_utime + p->p_user->u_cutime;
    p->p_stime = p->p_user->u_stime + p->p_user->u_cstime;
    p->p_flag &= ~(V7_STRC | V7_SULOCK);
    p->p_clktim = 0;
    p->p_stat = V7_SZOMB;
    struct proc *init = find_pid(1);
    if (init == p || (init && init->p_stat == V7_SZOMB)) init = NULL;
    v7_pid_t reaper = init ? 1 : 0;
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *q = &processes[i];
        if (q->p_stat != V7_UNUSED && q->p_ppid == p->p_pid) {
            q->p_ppid = reaper;
            if(q->p_stat==V7_SSTOP)q->p_stat=V7_SRUN;
        }
    }
    if (init) proc_wakeup(init);
    struct proc *parent = find_pid(p->p_ppid);
    if (parent) proc_wakeup(parent);
    switch_out();
    panic("zombie resumed");
}
static v7_pid_t collect(struct proc *parent, int32_t *status, bool *children)
{
    require_locked();
    *children = false;
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *p = &processes[i];
        if (p->p_stat == V7_UNUSED || p->p_ppid != parent->p_pid) continue;
        *children = true;
        if(p->p_stat==V7_SSTOP&&!(p->p_flag&V7_SWTED)){
            p->p_flag|=V7_SWTED;
            unsigned n=1;while(n<V7_NSIG&&!(p->p_sig&(1U<<(n-1))))++n;
            if(status)*status=(n<<8)|0177;
            return p->p_pid;
        }
        if (p->p_stat != V7_SZOMB) continue;
        if (p->p_vm.root) panic("reap before zombie resources released");
        v7_pid_t pid = p->p_pid;
        if (status) *status = p->p_xstat;
        parent->p_user->u_cutime += p->p_utime;
        parent->p_user->u_cstime += p->p_stime;
        clear(p->p_user, sizeof(*p->p_user));
        clear(p, sizeof(*p));
        return pid;
    }
    return -V7_ECHILD;
}
v7_pid_t proc_wait(int32_t *status)
{
    uintptr_t flags = proc_irq_save();
    if (!current) panic("wait without process");
    for (;;) {
        bool children;
        v7_pid_t result = collect(current, status, &children);
        if (result > 0 || !children) {
            if (result < 0) failure(V7_ECHILD);
            proc_irq_restore(flags);
            return result;
        }
        if (proc_sleep_interruptible(current, 30)) {
            proc_irq_restore(flags); return -V7_EINTR;
        }
    }
}
v7_pid_t proc_collect(int32_t *status)
{
    require_locked();
    if (current || scheduling) panic("bootstrap reap during scheduling");
    bool children;
    v7_pid_t result = collect(&processes[0], status, &children);
    return result < 0 && children ? -V7_EAGAIN : result;
}

void proc_user_interrupt(struct interrupt_frame *frame)
{
    if (!current || (current->p_flag & V7_SSYS) || frame->cs != USER_CS ||
        frame->ss != USER_DS || (frame->rflags & 0x3202) != 0x202 || !interrupt_stack_valid(frame))
        panic("invalid process user interrupt");
    current->p_user->u_ar0 = frame;
}
void proc_timer(struct interrupt_frame *frame)
{
    require_locked();
    if (!initialized) return;
    uint64_t now = timer_ticks();
    for (unsigned i = 1; i < V7_NPROC; ++i) {
        struct proc *p = &processes[i];
        if (p->p_clktim && now >= (uint64_t)p->p_clktim) {
            p->p_clktim = 0; signal_post(p, NX_SIGALRM);
        }
        if (p->p_stat == V7_SSLEEP && p->p_timed_sleep && (int64_t)(now - p->p_deadline) >= 0) {
            p->p_timed_sleep = false;
            p->p_wchan = NULL;
            p->p_stat = V7_SRUN;
        }
    }
    if (!current) return;
    if (!interrupt_stack_valid(frame)) panic("timer on wrong process stack");
    if ((frame->cs & 3) == 3) {
        ++current->p_user->u_utime;
        profile_sample(current,frame->rip,1);
    }
    else ++current->p_user->u_stime;
    if (++current->p_slice >= PROCESS_QUANTUM) {
        ++current->p_preemptions;
        proc_yield(); /* Resume inside this IRQ with IF=0; IRET restores IF. */
    }
}

/* Page-aligned initial heap follows the last ELF load segment. Growth rolls
 * back all acquired leaves on failure; shrinking never removes image pages. */
int64_t proc_brk(uintptr_t address)
{
    struct proc *p = current;
    if (!p->p_brk_base) return -V7_EINVAL;
    if (!address) return p->p_brk;
    if (address < p->p_brk_base || address >= EXEC_STACK_TOP - (EXEC_STACK_PAGES + 1) * PAGE_SIZE)
        return -V7_ENOMEM;
    uintptr_t old = (p->p_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uintptr_t end = (address + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uintptr_t cursor = old;
    for (; cursor < end; cursor += PAGE_SIZE) {
        uintptr_t pa = page_alloc();
        if (pa) clear((void *)pa, PAGE_SIZE);
        if (!pa || !vm_map(&p->p_vm, cursor, pa, VM_WRITE)) {
            if (pa && !page_free(pa)) panic("heap failed-page release");
            for (uintptr_t va = old; va < cursor; va += PAGE_SIZE) {
                uintptr_t physical;
                if (!vm_translate(&p->p_vm, va, &physical, NULL) ||
                    !vm_unmap(&p->p_vm, va) || !page_free(physical)) panic("heap rollback");
            }
            return -V7_ENOMEM;
        }
    }
    for (uintptr_t va = end; va < old; va += PAGE_SIZE) {
        uintptr_t pa;
        if (!vm_translate(&p->p_vm, va, &pa, NULL) ||
            !vm_unmap(&p->p_vm, va) || !page_free(pa)) panic("heap shrink");
    }
    p->p_size = p->p_size + end - old;
    p->p_user->u_dsize = p->p_user->u_exdata.data + p->p_user->u_exdata.bss + address - p->p_brk_base;
    p->p_brk = address;
    return 0;
}
