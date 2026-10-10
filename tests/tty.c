/* Exercise the production line discipline with a captured UART and checked
 * user-copy stubs; blocking/IRQ paths are exercised separately in QEMU. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tty.h"
#include "uaccess.h"
#include <nexix/errno.h>
static uint8_t output[32768], buffer[2048];
static size_t output_size;
static unsigned baud, parity, wakeups;
static bool receiver_enabled;
static struct address_space vm;
void serial_putc(uint8_t c) { assert(output_size < sizeof(output)); output[output_size++] = c; }
void serial_configure(unsigned rate, unsigned mode) { baud = rate; parity = mode; }
void serial_receive_enable(void) { receiver_enabled = true; }
unsigned proc_wakeup(void *channel) { assert(channel); ++wakeups; return 0; }
void proc_sleep(void *channel, unsigned priority)
{ (void)channel; (void)priority; fputs("unexpected empty/stopped tty in host test\n", stderr); abort(); }
_Noreturn void panic(const char *message) { fprintf(stderr, "panic: %s\n", message); abort(); }
int copyin(const struct address_space *space, uintptr_t source, void *dest, size_t size)
{ (void)space; if (source < 4096) return -NX_EFAULT; memcpy(dest, (const void *)source, size); return 0; }
int copyout(const struct address_space *space, uintptr_t dest, const void *source, size_t size)
{ (void)space; if (dest < 4096) return -NX_EFAULT; memcpy((void *)dest, source, size); return 0; }
static void feed(const void *bytes, size_t n)
{ const uint8_t *b = bytes; for (size_t i = 0; i < n; ++i) tty_receive(b[i]); }
static void expect(struct tty *t, const void *bytes, size_t n)
{
    memset(buffer, 0xa5, sizeof(buffer));
    assert(tty_io(t, &vm, (uintptr_t)buffer, sizeof(buffer), false) == (int64_t)n);
    assert(!memcmp(buffer, bytes, n));
}
static int control(struct tty *t, uint64_t request, void *argument)
{ return tty_ioctl(t, &vm, request, (uintptr_t)argument); }
int main(void)
{
    struct proc p = {.p_pgrp = 17};
    struct user u = {.u_procp = &p};
    struct tty *t = NULL;
    tty_init(); assert(receiver_enabled && baud == 115200 && !parity);
    assert(tty_open(0x999, &u, &t) == -NX_ENODEV && !t && tty_open_count() == 0);
    assert(tty_open(NX_CTTY_DEV, &u, &t) == -NX_ENXIO && !t);
    assert(tty_open(NX_CONSOLE_DEV, &u, &t) == 0 && t && tty_open_count() == 1);
    struct nx_sgttyb sg;
    assert(control(t, NX_TIOCGETP, &sg) == 0 && sg.flags == (NX_ECHO | NX_CRMOD));
    output_size = 0;
    feed("ab\bC\r", 5); expect(t, "aC\n", 3);
    assert(output_size == 8 && !memcmp(output, "ab\b \bC\r\n", 8));
    output_size = 0; feed("x\025z\n", 4); expect(t, "z\n", 2);
    assert(output_size == 6 && !memcmp(output, "x\r\nz\r\n", 6));
    sg.flags &= ~NX_ECHO;
    assert(control(t, NX_TIOCSETP, &sg) == 0);
    feed("a\nb\n", 4); expect(t, "a\n", 2); expect(t, "b\n", 2);
    feed("hello\n", 6);
    assert(tty_io(t, &vm, (uintptr_t)buffer, 2, false) == 2 && !memcmp(buffer, "he", 2));
    expect(t, "llo\n", 4);
    feed("\004xy\004", 4); expect(t, "", 0); expect(t, "xy", 2);
    /* SETN preserves an unfinished cooked line when only echo changes. */
    feed("pending", 7);
    assert(control(t, NX_TIOCSETN, &sg) == 0);
    feed("\n", 1); expect(t, "pending\n", 8);
    feed("preserved\n", 10);
    assert(control(t, NX_TIOCSETP, (void *)1) == -NX_EFAULT);
    sg.flags |= 0x80000000U;
    assert(control(t, NX_TIOCSETP, &sg) == -NX_EINVAL);
    sg.flags &= ~0x80000000U;
    sg.ispeed = NX_B9600;
    assert(control(t, NX_TIOCSETP, &sg) == -NX_EINVAL);
    sg.ispeed = sg.ospeed = NX_B0;
    assert(control(t, NX_TIOCSETP, &sg) == -NX_EINVAL);
    sg.ispeed = sg.ospeed = NX_B115200;
    expect(t, "preserved\n", 10);
    assert(baud == 115200);
    /* Capacity limits preserve whole records and leave space for newline. */
    for (unsigned i = 0; i < 300; ++i) tty_receive('X');
    tty_receive('\n');
    assert(tty_io(t, &vm, (uintptr_t)buffer, sizeof(buffer), false) == TTY_LINE_CAPACITY);
    for (unsigned i = 0; i < TTY_LINE_CAPACITY - 1; ++i) assert(buffer[i] == 'X');
    assert(buffer[TTY_LINE_CAPACITY - 1] == '\n');
    feed("Z\n", 2); expect(t, "Z\n", 2);
    for (unsigned i = 0; i < TTY_INPUT_CAPACITY / 2; ++i) feed("A\n", 2);
    feed("dropped\n", 8);
    for (unsigned i = 0; i < TTY_INPUT_CAPACITY / 2; ++i) expect(t, "A\n", 2);
    feed("C\n", 2); expect(t, "C\n", 2);
    struct nx_tchars tc;
    assert(control(t, NX_TIOCGETC, &tc) == 0);
    tc.eof = '!'; tc.brk = ';';
    assert(control(t, NX_TIOCSETC, &tc) == 0);
    feed("abc;q!!", 7); expect(t, "abc;", 4); expect(t, "q", 1); expect(t, "", 0);
    sg.flags = NX_CBREAK | NX_CRMOD;
    assert(control(t, NX_TIOCSETN, &sg) == 0);
    const uint8_t seven[] = {0xe1, '\r'};
    feed(seven, 2); expect(t, "a\n", 2);
    feed("\004", 1); expect(t, "\004", 1);
    feed("discard", 7);
    sg.flags = NX_RAW;
    assert(control(t, NX_TIOCSETN, &sg) == 0); /* mode change flushes */
    const uint8_t binary[] = {0, 0x80, 0xff, 4, 17, 19, '\r'};
    feed(binary, sizeof(binary)); expect(t, binary, sizeof(binary));
    for (unsigned i = 0; i <= TTY_INPUT_CAPACITY; ++i) tty_receive(i);
    assert(tty_io(t, &vm, (uintptr_t)buffer, sizeof(buffer), false) == TTY_INPUT_CAPACITY);
    for (unsigned i = 0; i < TTY_INPUT_CAPACITY; ++i) assert(buffer[i] == (uint8_t)i);
    /* Raw ring wrapping and partial reads preserve order. */
    for (unsigned i = 0; i < 400; ++i) tty_receive(i);
    assert(tty_io(t, &vm, (uintptr_t)buffer, 300, false) == 300);
    for (unsigned i = 0; i < 300; ++i) assert(buffer[i] == (uint8_t)i);
    for (unsigned i = 400; i < 1100; ++i) tty_receive(i);
    assert(tty_io(t, &vm, (uintptr_t)buffer, sizeof(buffer), false) == 800);
    for (unsigned i = 0; i < 800; ++i) assert(buffer[i] == (uint8_t)(i + 300));
    output_size = 0;
    assert(tty_io(t, &vm, (uintptr_t)binary, sizeof(binary), true) == sizeof(binary));
    assert(output_size == sizeof(binary) && !memcmp(output, binary, sizeof(binary)));
    sg.flags = NX_CRMOD | NX_XTABS | NX_ODDP;
    sg.ispeed = sg.ospeed = NX_B9600;
    assert(control(t, NX_TIOCSETP, &sg) == 0 && baud == 9600 && parity == 1);
    output_size = 0;
    assert(tty_io(t, &vm, (uintptr_t)"ab\tc\n", 5, true) == 5);
    assert(output_size == 11 && !memcmp(output, "ab      c\r\n", 11));
    tty_receive(023); /* XOFF, followed by SETP must release software stop. */
    assert(control(t, NX_TIOCSETP, &sg) == 0);
    assert(tty_io(t, &vm, (uintptr_t)"x", 1, true) == 1);
    output_size = 0;
    const uint8_t high = 0xe1;
    assert(tty_io(t, &vm, (uintptr_t)&high, 1, true) == 1 && output_size == 1 && output[0] == 'a');
    sg.flags = NX_EVENP;
    assert(control(t, NX_TIOCSETN, &sg) == 0 && parity == 2);
    sg.flags = NX_ODDP | NX_EVENP;
    assert(control(t, NX_TIOCSETN, &sg) == 0 && !parity);
    sg.flags = NX_RAW | NX_ODDP;
    assert(control(t, NX_TIOCSETN, &sg) == 0 && !parity);
    sg.flags = 002000; /* partial tab delay is not XTABS */
    assert(control(t, NX_TIOCSETN, &sg) == -NX_EINVAL);
    feed("old", 3); assert(control(t, NX_TIOCFLUSH, NULL) == 0);
    feed("new", 3); expect(t, "new", 3);
    assert(control(t, NX_TIOCGETP, (void *)1) == -NX_EFAULT);
    assert(control(t, UINT64_MAX, NULL) == -NX_ENOTTY);
    int32_t discipline = -1;
    assert(control(t, NX_TIOCGETD, &discipline) == 0 && !discipline);
    discipline = 1;
    assert(control(t, NX_TIOCSETD, &discipline) == -NX_ENXIO);
    struct tty *other;
    assert(tty_open(NX_CTTY_DEV, &u, &other) == 0 && other == t && tty_open_count() == 2);
    tty_close(other); assert(tty_open_count() == 1);
    tty_close(t); assert(!tty_open_count());
    tty_receive('x');
    assert(tty_open(NX_CONSOLE_DEV, &u, &t) == 0 && baud == 115200);
    assert(control(t, NX_TIOCGETP, &sg) == 0 && sg.flags == (NX_ECHO | NX_CRMOD));
    feed("fresh\n", 6); expect(t, "fresh\n", 6);
    tty_close(t);
    assert(wakeups > 0);
    puts("PASS: TTY line editing, records/EOF, queue bounds/reuse, raw/cbreak, echo/output and ioctl validation");
    return 0;
}
