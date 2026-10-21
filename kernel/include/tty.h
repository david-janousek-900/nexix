#ifndef NEXIX_TTY_H
#define NEXIX_TTY_H
#include "process.h"
#include "boot.h"
#include <nexix/tty.h>
#define TTY_INPUT_CAPACITY 1024
#define TTY_LINE_CAPACITY 256
#define TTY_LINES 3
struct tty_driver {
    void *context;
    bool (*enqueue)(void *,const uint8_t *,size_t);
    bool (*idle)(void *);
    void (*flush)(void *);
    void (*format)(void *,unsigned,uint8_t);
    void (*outputs)(void *,bool,bool);
    unsigned (*status)(void *);
};
int tty_attach(unsigned,const struct tty_driver *);
void tty_receive_line(unsigned,uint8_t,unsigned);
void tty_modem_line(unsigned,unsigned);
/* Independent bounded terminals. All entry points run with IRQs disabled. Device receive
 * never sleeps; readers/writers use process sleep/wakeup on private channels. */
/* Physical line backing /dev/console; set at boot before any opens. */
extern unsigned tty_console_line;
void tty_init(void);
void tty_set_group(struct tty *, v7_pid_t old, v7_pid_t group, bool privileged);
int tty_open(v7_dev_t device, struct user *u, struct tty **result);
void tty_close(struct tty *);
bool tty_handle_valid(struct tty *);
unsigned tty_open_count(void);
void tty_receive(uint8_t byte);
void tty_receive_status(uint8_t,unsigned errors);
void tty_timer(uint64_t now);
void tty_modem_status(unsigned);
int tty_modem_peer(unsigned);
/* File layer preflights the entire user buffer before entry/sleep. */
int64_t tty_io(struct tty *, struct address_space *, uintptr_t buffer, size_t count, bool write);
int tty_ioctl(struct tty *, struct address_space *, uint64_t request, uintptr_t argument);
/* Multiplexor hooks: no sleeping or user pointers in the kernel-buffer path. */
int tty_mpx_bind(struct tty *,void *owner);
void tty_mpx_unbind(struct tty *,void *owner);
bool tty_mpx_ready(struct tty *);
size_t tty_mpx_read(struct tty *,uint8_t *,size_t,bool *eof);
int64_t tty_mpx_write(struct tty *,const uint8_t *,size_t);
int tty_control_size(uint64_t,bool *get);
int tty_control(struct tty *,uint64_t,void *);
void mpx_tty_notify(void *owner,unsigned signal);
void mpx_tty_resume(struct tty *);
void tty_selftest(enum boot_test test);
bool tty_test_gate(struct interrupt_frame *);
#endif
