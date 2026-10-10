#ifndef NEXIX_USER_TEST_H
#define NEXIX_USER_TEST_H
#include "boot.h"
#include "interrupts.h"
void user_mode_selftest(enum boot_test test);
void user_mode_interrupt(struct interrupt_frame *frame);
void user_mode_test_gate(struct interrupt_frame *frame);
#endif
