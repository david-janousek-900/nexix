#ifndef NEXIX_PROCESS_H
#define NEXIX_PROCESS_H
#include "v7/core.h"
#include "interrupts.h"
#define PROCESS_QUANTUM 5U
/* Single CPU. Disable interrupts across condition checks and proc_sleep to
 * prevent lost wakeups. Each switching operation restores its caller's IF. */
uintptr_t proc_irq_save(void);
void proc_irq_restore(uintptr_t flags);
void proc_init(void);
struct proc *proc_current(void);
const struct proc *proc_table(void);
/* Reserve unpublished SIDL process; owns a private VM and a stack-pool slot.
 * IRQs must be disabled when using builders or examining the process table. */
struct proc *proc_alloc(void);
void proc_abort(struct proc *p);
bool proc_map_page(struct proc *p, uintptr_t address, unsigned permissions,
                   const void *contents); /* contents: NULL or one full page */
bool proc_start_user(struct proc *p, uintptr_t entry, uintptr_t stack);
v7_pid_t proc_spawn(void (*entry)(void *), void *argument);
void proc_run(void); /* Bootstrap scheduler: returns when only zombies remain. */
void proc_yield(void);
void proc_sleep(void *channel, unsigned priority);
unsigned proc_wakeup(void *channel);
void proc_delay(uint64_t ticks);
/* Fork a live CPL3 frame; parent gets PID, child's RAX is zero. Negative errno
 * on failure. File/inode/shared-text ownership is deferred to filesystem work. */
v7_pid_t proc_fork(const struct interrupt_frame *frame);
_Noreturn void proc_exit(int32_t status); /* V7-encoded wait status */
v7_pid_t proc_wait(int32_t *status); /* kernel pointer, optional; -ECHILD */
/* Bootstrap collects its zombies after proc_run, without a sleep context. */
v7_pid_t proc_collect(int32_t *status);
void proc_timer(struct interrupt_frame *frame);
void proc_user_interrupt(struct interrupt_frame *frame);
void process_selftest(void);
bool process_test_gate(struct interrupt_frame *frame);
#endif
