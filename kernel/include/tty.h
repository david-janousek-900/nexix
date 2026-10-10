#ifndef NEXIX_TTY_H
#define NEXIX_TTY_H
#include "process.h"
#include "boot.h"
#include <nexix/tty.h>
#define TTY_INPUT_CAPACITY 1024
#define TTY_LINE_CAPACITY 256
/* Single COM1 terminal. All entry points run with IRQs disabled. UART receive
 * never sleeps; readers/writers use process sleep/wakeup on private channels. */
void tty_init(void);
int tty_open(v7_dev_t device, struct user *u, struct tty **result);
void tty_close(struct tty *);
unsigned tty_open_count(void);
void tty_receive(uint8_t byte);
/* File layer preflights the entire user buffer before entry/sleep. */
int64_t tty_io(struct tty *, struct address_space *, uintptr_t buffer, size_t count, bool write);
int tty_ioctl(struct tty *, struct address_space *, uint64_t request, uintptr_t argument);
void tty_selftest(enum boot_test test);
bool tty_test_gate(struct interrupt_frame *);
#endif
