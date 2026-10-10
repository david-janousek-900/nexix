#ifndef NEXIX_LIBC_TEST_H
#define NEXIX_LIBC_TEST_H
#include "interrupts.h"
#include <stdbool.h>
void libc_selftest(void);
bool libc_test_gate(struct interrupt_frame *);
#endif
