#ifndef NEXIX_SIGNALS_H
#define NEXIX_SIGNALS_H
#include "process.h"
#include <nexix/signal.h>
void signal_post(struct proc *, unsigned);
void signal_group(v7_pid_t, unsigned);
/* User-requested group delivery applies the same UID checks as kill. */
int signal_group_user(v7_pid_t, unsigned);
bool signal_pending(struct proc *);
void signal_deliver(struct interrupt_frame *);
int64_t signal_install(uint64_t, uintptr_t, uintptr_t);
int signal_kill(int64_t, uint64_t);
int signal_return(uintptr_t, struct interrupt_frame *);
void signals_selftest(void);
#endif
