#ifndef NEXIX_KERNEL_SYSCALL_H
#define NEXIX_KERNEL_SYSCALL_H
#include "interrupts.h"
#include <stdbool.h>
int64_t syscall_fallback(uint64_t number);
void syscall_dispatch(struct interrupt_frame *frame);
void syscall_selftest(void);
bool syscall_test_gate(struct interrupt_frame *frame);
#endif
