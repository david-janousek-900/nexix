#include "console.h"
#include "process.h"
#include "callout.h"
#include "segments.h"
#include "serial.h"
#define REQUIRE(c) do { if (!(c)) panic("process self-test: " #c); } while (0)
extern bool arch_context_probe(void);
extern const uint8_t process_blob_start[], process_blob_end[];
static volatile unsigned progress[2], armed, awakened;
static char channels[2];
static bool active_gate, fork_seen, parent_exit_seen, child_exit_seen;
static v7_pid_t fork_parent, fork_child;
static uint64_t preemptions[2];
static uint8_t code_page[PAGE_SIZE];
static volatile unsigned callout_seen;
static void callout_wake(void *argument)
{
    REQUIRE(argument==(void *)&callout_seen);
    ++callout_seen;proc_wakeup(argument);
}

static void worker(void *argument)
{
    unsigned index = (uintptr_t)argument;
    REQUIRE(arch_context_probe());
    /* Neither worker yields in this handshake: only PIT preemption can finish. */
    for (unsigned n = 1; n <= 3; ++n) {
        progress[index] = n;
        while (progress[1 - index] < n) __asm__ volatile ("pause" : : : "memory");
    }
    preemptions[index] = proc_current()->p_preemptions;
    REQUIRE(preemptions[index]);
    proc_exit((int32_t)(index + 1) << 8);
}
static void sleeper(void *argument)
{
    uintptr_t flags = proc_irq_save();
    ++armed;
    proc_sleep(argument, 26);
    ++awakened;
    proc_irq_restore(flags);
    proc_exit(0);
}
static void grandchild(void *argument)
{
    (void)argument;
    proc_delay(3);
    proc_exit(0x4400);
}
static void orphan_parent(void *argument)
{
    (void)argument;
    REQUIRE(proc_spawn(grandchild, NULL) > 0);
    proc_exit(0x3300);
}
static void kernel_tests(void *argument)
{
    (void)argument;
    int32_t status;
    uintptr_t initial_flags = proc_irq_save();
    REQUIRE(initial_flags & 0x200);
    REQUIRE(arch_context_probe());
    REQUIRE(!(proc_irq_save() & 0x200));
    proc_irq_restore(initial_flags);
    REQUIRE(proc_wait(&status) == -V7_ECHILD);
    REQUIRE(proc_spawn(worker, (void *)0) > 0);
    REQUIRE(proc_spawn(worker, (void *)1) > 0);
    unsigned statuses = 0;
    for (unsigned i = 0; i < 2; ++i) {
        REQUIRE(proc_wait(&status) > 0);
        REQUIRE(status == 0x100 || status == 0x200);
        statuses |= (unsigned)status;
    }
    REQUIRE(statuses == 0x300 && progress[0] == 3 && progress[1] == 3);
    REQUIRE(proc_current()->p_user->u_cstime > 0);
    console_puts("PROCESS SWITCH OK\r\n");
    console_puts("PROCESS PREEMPT OK\r\n");

    REQUIRE(proc_spawn(sleeper, &channels[0]) > 0);
    REQUIRE(proc_spawn(sleeper, &channels[0]) > 0);
    REQUIRE(proc_spawn(sleeper, &channels[1]) > 0);
    while (armed != 3) proc_yield();
    REQUIRE(awakened == 0);
    REQUIRE(proc_wakeup(&channels[0]) == 2);
    REQUIRE(proc_wakeup(&channels[0]) == 0);
    for (unsigned i = 0; i < 2; ++i) REQUIRE(proc_wait(&status) > 0 && status == 0);
    REQUIRE(awakened == 2);
    REQUIRE(proc_wakeup(&channels[1]) == 1);
    REQUIRE(proc_wait(&status) > 0 && status == 0 && awakened == 3);
    uint64_t before = timer_ticks();
    proc_delay(3); /* All tasks are sleeping: exercise scheduler STI/HLT wakeup. */
    REQUIRE(timer_ticks() - before >= 3);
    console_puts("PROCESS SLEEP OK\r\n");
    uintptr_t callout_flags=proc_irq_save();
    before=timer_ticks();
    REQUIRE(callout_schedule(callout_wake,(void *)&callout_seen,2)==0);
    while(!callout_seen)proc_sleep((void *)&callout_seen,26);
    REQUIRE(callout_seen==1 && timer_ticks()-before>=2);
    REQUIRE(callout_cancel(callout_wake,(void *)&callout_seen)==0);
    proc_irq_restore(callout_flags);
    console_puts("CALLOUT IRQ OK\r\n");

    REQUIRE(proc_spawn(orphan_parent, NULL) > 0);
    statuses = 0;
    for (unsigned i = 0; i < 2; ++i) {
        REQUIRE(proc_wait(&status) > 0);
        REQUIRE(status == 0x3300 || status == 0x4400);
        statuses |= status;
    }
    REQUIRE(statuses == 0x7700 && proc_wait(NULL) == -V7_ECHILD);
    console_puts("PROCESS WAIT OK\r\n");

    /* Reserve every free slot; failure and abort must not leak VM roots. */
    uintptr_t flags = proc_irq_save();
    uint32_t pages = page_free_count();
    struct proc *reserved[V7_NPROC];
    unsigned count = 0;
    struct proc *p;
    while ((p = proc_alloc())) reserved[count++] = p;
    REQUIRE(count == V7_NPROC - 2);
    REQUIRE(proc_current()->p_user->u_error == V7_EAGAIN);
    for (unsigned i = 0; i < count; ++i) proc_abort(reserved[i]);
    REQUIRE(page_free_count() == pages);
    proc_current()->p_uid = proc_current()->p_user->u_uid = 77;
    count = 0;
    while ((p = proc_alloc())) reserved[count++] = p;
    REQUIRE(count == 24 && proc_current()->p_user->u_error == V7_EAGAIN);
    for (unsigned i = 0; i < count; ++i) proc_abort(reserved[i]);
    proc_current()->p_uid = proc_current()->p_user->u_uid = 0;
    REQUIRE(page_free_count() == pages);
    proc_irq_restore(flags);
    console_puts("PROCESS SLOTS OK\r\n");
}

static void fork_oom_test(struct interrupt_frame *frame)
{
    uint32_t before = page_free_count();
    uintptr_t head = 0, page;
    while ((page = page_alloc())) {
        *(uintptr_t *)page = head;
        head = page;
    }
    /* This six-leaf image requires 16 pages to clone. Test every earlier
     * allocation boundary, including rollback after successful leaf copies. */
    for (unsigned available = 0; available < 16; ++available) {
        REQUIRE(page_free_count() == available);
        REQUIRE(proc_fork(frame) == -V7_ENOMEM);
        REQUIRE(page_free_count() == available);
        if (available != 15) {
            REQUIRE(head);
            uintptr_t next = *(uintptr_t *)head;
            REQUIRE(page_free(head));
            head = next;
        }
    }
    while (head) {
        uintptr_t next = *(uintptr_t *)head;
        REQUIRE(page_free(head));
        head = next;
    }
    REQUIRE(page_free_count() == before);
    console_puts("PROCESS OOM OK\r\n");
}

bool process_test_gate(struct interrupt_frame *frame)
{
    if (!active_gate || !proc_current()) return false;
    struct proc *parent = proc_current();
    REQUIRE(frame->cs == USER_CS && frame->ss == USER_DS);
    switch (frame->rax) {
    case 0x100: {
        REQUIRE(!fork_seen && parent->p_pid == fork_parent);
        struct file unsupported_file = {0};
        uint32_t unchanged = page_free_count();
        parent->p_user->u_ofile[0] = &unsupported_file;
        REQUIRE(proc_fork(frame) == -V7_EBUSY && page_free_count() == unchanged);
        parent->p_user->u_ofile[0] = NULL;
        fork_oom_test(frame);
        parent->p_user->u_error = 0;
        uint32_t before = page_free_count();
        v7_pid_t child_pid = proc_fork(frame);
        REQUIRE(child_pid > 0);
        REQUIRE(page_free_count() == before - 16);
        struct proc *child = NULL;
        const struct proc *table = proc_table();
        for (unsigned i = 1; i < V7_NPROC; ++i)
            if (table[i].p_stat != V7_UNUSED && table[i].p_pid == child_pid)
                child = (struct proc *)&table[i];
        REQUIRE(child && child->p_ppid == parent->p_pid);
        REQUIRE(child->p_user != parent->p_user && child->p_user->u_procp == child);
        REQUIRE(child->p_user->u_ap == child->p_user->u_arg);
        REQUIRE(child->p_uid == 123 && child->p_user->u_uid == 123);
        REQUIRE(child->p_user->u_gid == 456 && child->p_user->u_rgid == 456);
        REQUIRE(child->p_user->u_signal[1] == UINT64_C(0x123456789ab));
        REQUIRE(child->p_stack_top != parent->p_stack_top);
        REQUIRE(child->p_user->u_ar0->rax == 0);
        REQUIRE(child->p_size == 6 * PAGE_SIZE);
        uintptr_t addresses[] = { USER_BASE, USER_BASE + 2 * PAGE_SIZE,
            USER_BASE + 4 * PAGE_SIZE, USER_BASE + 5 * PAGE_SIZE,
            1UL << 39, USER_LIMIT - PAGE_SIZE };
        for (unsigned i = 0; i < 6; ++i) {
            uintptr_t original, duplicate;
            unsigned a, b;
            REQUIRE(vm_translate(&parent->p_vm, addresses[i], &original, &a));
            REQUIRE(vm_translate(&child->p_vm, addresses[i], &duplicate, &b));
            REQUIRE(original != duplicate && a == b);
            for (unsigned n = 0; n < PAGE_SIZE; ++n)
                REQUIRE(((uint8_t *)original)[n] == ((uint8_t *)duplicate)[n]);
        }
        fork_seen = true;
        fork_child = child_pid;
        frame->rax = child_pid;
        return true;
    }
    case 0x101: {
        REQUIRE(parent->p_pid == fork_parent);
        int32_t status = 0;
        frame->rax = proc_wait(&status);
        frame->rdx = status;
        REQUIRE((v7_pid_t)frame->rax == fork_child && status == 0x2a00);
        REQUIRE(parent->p_user->u_cutime >= 10);
        return true;
    }
    case 0x102:
        REQUIRE(parent->p_user->u_utime >= 10 && parent->p_preemptions >= 2);
        if (parent->p_pid == fork_parent) {
            REQUIRE(frame->rdi == 0 && !parent_exit_seen);
            parent_exit_seen = true;
        } else {
            REQUIRE(parent->p_pid == fork_child && frame->rdi == 0x2a00 && !child_exit_seen);
            child_exit_seen = true;
        }
        proc_exit(frame->rdi);
    case 0x104:
        frame->rax = parent->p_user->u_utime;
        return true;
    default:
        panic("process test reached failure gate");
    }
}

void process_selftest(void)
{
    uint32_t before = page_free_count();
    proc_init();
    v7_pid_t root = proc_spawn(kernel_tests, NULL);
    REQUIRE(root == 1);
    proc_run();
    int32_t status;
    REQUIRE(proc_collect(&status) == root && status == 0);
    REQUIRE(proc_collect(NULL) == -V7_ECHILD);
    REQUIRE(page_free_count() == before);

    size_t length = (uintptr_t)process_blob_end - (uintptr_t)process_blob_start;
    REQUIRE(length < PAGE_SIZE);
    for (size_t i = 0; i < length; ++i) code_page[i] = process_blob_start[i];
    struct proc *p = proc_alloc();
    REQUIRE(p);
    unsigned stack_flags;
    REQUIRE(!vm_translate(&p->p_vm, p->p_stack_bottom - PAGE_SIZE, NULL, NULL));
    REQUIRE(vm_translate(&p->p_vm, p->p_stack_bottom, NULL, &stack_flags));
    REQUIRE(stack_flags == VM_WRITE); /* Supervisor RW/NX kernel stack. */
    REQUIRE(proc_map_page(p, USER_BASE, VM_EXEC, code_page));
    REQUIRE(proc_map_page(p, USER_BASE + 2 * PAGE_SIZE, VM_WRITE, NULL));
    REQUIRE(proc_map_page(p, USER_BASE + 4 * PAGE_SIZE, VM_WRITE, NULL));
    REQUIRE(proc_map_page(p, USER_BASE + 5 * PAGE_SIZE, 0, NULL));
    REQUIRE(proc_map_page(p, 1UL << 39, VM_WRITE, NULL));
    REQUIRE(proc_map_page(p, USER_LIMIT - PAGE_SIZE, VM_WRITE, NULL));
    uintptr_t data;
    REQUIRE(vm_translate(&p->p_vm, USER_BASE + 4 * PAGE_SIZE, &data, NULL));
    *(uint64_t *)data = 0x1111;
    REQUIRE(!proc_start_user(p, USER_BASE + 4 * PAGE_SIZE, USER_BASE + 3 * PAGE_SIZE));
    REQUIRE(proc_start_user(p, USER_BASE, USER_BASE + 3 * PAGE_SIZE));
    p->p_uid = p->p_user->u_uid = p->p_user->u_ruid = 123;
    p->p_user->u_gid = p->p_user->u_rgid = 456;
    p->p_user->u_signal[1] = UINT64_C(0x123456789ab);
    fork_parent = p->p_pid;
    active_gate = true;
    proc_run();
    active_gate = false;
    REQUIRE(fork_seen && parent_exit_seen && child_exit_seen);
    REQUIRE(proc_collect(&status) == fork_parent && status == 0);
    REQUIRE(proc_collect(NULL) == -V7_ECHILD);
    REQUIRE(page_free_count() == before);
    for (unsigned i = 1; i < V7_NPROC; ++i) REQUIRE(proc_table()[i].p_stat == V7_UNUSED);
    console_puts("PROCESS FORK OK\r\n");
    console_puts("PROCESS CLEANUP OK\r\n");
}
