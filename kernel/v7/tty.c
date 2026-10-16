/* Native adaptation of V7 dev/tty.c's line discipline for the COM1 UART. */
#include "tty.h"
#include "serial.h"
#include <nexix/kinfo.h>
static uint64_t tty_in,tty_out;
void tty_statistics(struct nx_kinfo_io *r){r->tty_in=tty_in;r->tty_out=tty_out;}
static void counted_putc(char c){serial_putc(c);++tty_out;}
#include "uaccess.h"
#include "signals.h"
#include <nexix/errno.h>
#define RECORD_END 0x100
#define EMPTY_EOF 0x200
struct tty {
    struct nx_sgttyb settings;
    struct nx_tchars chars;
    uint16_t input[TTY_INPUT_CAPACITY];
    uint8_t line[TTY_LINE_CAPACITY];
    unsigned head, count, length, opens, column;
    uint64_t dropped;
    v7_pid_t group;
    bool stopped;
    char input_channel, output_channel;
};
static struct tty console;
static const unsigned rates[] = {0, 50, 75, 110, 134, 150, 200, 300, 600,
    1200, 1800, 2400, 4800, 9600, 19200, 38400, 115200};
static bool immediate(const struct tty *t) { return t->settings.flags & (NX_RAW | NX_CBREAK); }
static void flush(struct tty *t)
{ t->head = t->count = t->length = 0; }
static void resume(struct tty *t)
{ t->stopped = false; proc_wakeup(&t->output_channel); }
static void defaults(struct tty *t)
{
    t->settings = (struct nx_sgttyb){NX_B115200, NX_B115200, 0177, 025, NX_ECHO | NX_CRMOD};
    t->chars = (struct nx_tchars){003, 034, 021, 023, 004, 0377};
    flush(t); resume(t); t->column = 0;
    serial_configure(115200, 0);
}
void tty_init(void)
{
    defaults(&console);
    serial_receive_enable();
}
void tty_set_group(struct tty *t, v7_pid_t group)
{ if (t == &console) t->group = group; }
unsigned tty_open_count(void) { return console.opens; }
int tty_open(v7_dev_t dev, struct user *u, struct tty **out)
{
    if (dev != NX_CONSOLE_DEV && dev != NX_CTTY_DEV) return -NX_ENODEV;
    if (dev == NX_CTTY_DEV && u->u_ttyp != &console) return -NX_ENXIO;
    if (!console.opens) defaults(&console);
    ++console.opens;
    if (!u->u_ttyp) { u->u_ttyp = &console; u->u_ttyd = NX_CONSOLE_DEV; }
    /* V7 ttyopen: a process without a group joins the terminal group,
     * creating it from its PID on the first open. Keep init outside it. */
    if (!u->u_procp->p_pgrp) {
        if (!console.group) console.group = u->u_procp->p_pid;
        u->u_procp->p_pgrp = console.group;
    }
    if (!console.group) console.group = u->u_procp->p_pgrp;
    *out = &console;
    return 0;
}
void tty_close(struct tty *t)
{
    if (t != &console || !t->opens) panic("invalid tty close");
    if (!--t->opens) { flush(t); resume(t); t->group = 0; }
}
static void output(struct tty *t, uint8_t c)
{
    if (!(t->settings.flags & NX_RAW)) {
        c &= 0177;
        if (c == '\n' && (t->settings.flags & NX_CRMOD)) counted_putc('\r');
        if (c == '\t' && (t->settings.flags & NX_XTABS) == NX_XTABS) {
            unsigned spaces = 8 - t->column % 8;
            while (spaces--) { counted_putc(' '); ++t->column; }
            return;
        }
    }
    counted_putc(c);
    if (c == '\r' || c == '\n') t->column = 0;
    else if (c == '\b') { if (t->column) --t->column; }
    else if (c == '\t') t->column += 8 - t->column % 8;
    else if (c >= ' ' && c != 0177) ++t->column;
}
static void echo(struct tty *t, uint8_t c)
{ if ((t->settings.flags & NX_ECHO) && !t->stopped) output(t, c); }
static void enqueue(struct tty *t, uint16_t value)
{
    t->input[(t->head + t->count) % TTY_INPUT_CAPACITY] = value;
    ++t->count;
}
static void commit(struct tty *t)
{
    unsigned need = t->length ? t->length : 1;
    if (TTY_INPUT_CAPACITY - t->count < need) { t->dropped += need; t->length = 0; return; }
    if (!t->length) enqueue(t, RECORD_END | EMPTY_EOF);
    else {
        for (unsigned i = 0; i < t->length; ++i)
            enqueue(t, t->line[i] | (i + 1 == t->length ? RECORD_END : 0));
    }
    t->length = 0;
    proc_wakeup(&t->input_channel);
}
void tty_receive(uint8_t c)
{
    ++tty_in;
    struct tty *t = &console;
    if (!t->opens) return;
    if (!(t->settings.flags & NX_RAW)) {
        c &= 0177;
        if ((c == t->chars.intr || c == t->chars.quit) && c != 0377) {
            flush(t); resume(t);
            signal_group(t->group, c == t->chars.intr ? NX_SIGINT : NX_SIGQUIT);
            proc_wakeup(&t->input_channel); return;
        }
        if (c == t->chars.start && c != 0377) { resume(t); return; }
        if (c == t->chars.stop && c != 0377) { t->stopped = true; return; }
        if (c == '\r' && (t->settings.flags & NX_CRMOD)) c = '\n';
    }
    if (immediate(t)) {
        if (t->count == TTY_INPUT_CAPACITY) { ++t->dropped; return; }
        enqueue(t, c); echo(t, c); proc_wakeup(&t->input_channel); return;
    }
    if (c == t->settings.erase || (t->settings.erase == 0177 && c == '\b')) {
        if (t->length) {
            --t->length;
            echo(t, '\b'); echo(t, ' '); echo(t, '\b');
        }
        return;
    }
    if (c == t->settings.kill) { t->length = 0; echo(t, '\n'); return; }
    if (c == t->chars.eof) { commit(t); return; }
    bool delimiter = c == '\n' || (t->chars.brk != 0377 && c == t->chars.brk);
    /* Reserve a byte for a delimiter, so an overlong line remains readable. */
    if (t->length >= TTY_LINE_CAPACITY - (delimiter ? 0 : 1)) { ++t->dropped; return; }
    t->line[t->length++] = c; echo(t, c);
    if (delimiter) commit(t);
}
int64_t tty_io(struct tty *t, struct address_space *vm, uintptr_t address, size_t count, bool write)
{
    if (!count) return 0;
    size_t done = 0;
    if (write) {
        uint8_t bytes[128];
        while (done < count) {
            size_t n = count - done; if (n > sizeof(bytes)) n = sizeof(bytes);
            if (copyin(vm, address + done, bytes, n)) panic("tty write mapping changed");
            for (size_t i = 0; i < n; ++i) {
                while (t->stopped) if (proc_sleep_interruptible(&t->output_channel, 29))
                    return done + i ? (int64_t)(done + i) : -NX_EINTR;
                output(t, bytes[i]);
            }
            done += n;
        }
        return done;
    }
    while (!t->count) if (proc_sleep_interruptible(&t->input_channel, 28)) return -NX_EINTR;
    while (done < count && t->count) {
        uint16_t value = t->input[t->head];
        t->head = (t->head + 1) % TTY_INPUT_CAPACITY; --t->count;
        if (!(value & EMPTY_EOF)) {
            uint8_t byte = value;
            if (copyout(vm, address + done, &byte, 1)) panic("tty read mapping changed");
            ++done;
        }
        if (!immediate(t) && (value & RECORD_END)) break;
    }
    return done;
}
int tty_ioctl(struct tty *t, struct address_space *vm, uint64_t request, uintptr_t address)
{
    switch (request) {
    case NX_TIOCGETP: return copyout(vm, address, &t->settings, sizeof(t->settings));
    case NX_TIOCSETP: case NX_TIOCSETN: {
        struct nx_sgttyb settings;
        int error = copyin(vm, address, &settings, sizeof(settings));
        if (error) return error;
        uint32_t allowed = NX_CBREAK | NX_ECHO | NX_CRMOD | NX_RAW | NX_ODDP | NX_EVENP | NX_XTABS;
        if (settings.flags & ~allowed || !settings.ospeed || settings.ospeed >= sizeof(rates) / sizeof(rates[0]) ||
            settings.ispeed != settings.ospeed ||
            ((settings.flags & NX_XTABS) && (settings.flags & NX_XTABS) != NX_XTABS)) return -NX_EINVAL;
        /* Mode changes cannot reinterpret queued cooked record markers as bytes. */
        if (request == NX_TIOCSETP || ((settings.flags ^ t->settings.flags) & (NX_RAW | NX_CBREAK))) flush(t);
        t->settings = settings;
        unsigned parity = settings.flags & (NX_ODDP | NX_EVENP);
        if (settings.flags & NX_RAW || parity == (NX_ODDP | NX_EVENP)) parity = 0;
        serial_configure(rates[settings.ospeed], parity == NX_ODDP ? 1 : parity == NX_EVENP ? 2 : 0);
        if (request == NX_TIOCSETP || (settings.flags & NX_RAW)) resume(t);
        return 0;
    }
    case NX_TIOCGETC: return copyout(vm, address, &t->chars, sizeof(t->chars));
    case NX_TIOCSETC: {
        struct nx_tchars chars;
        int error = copyin(vm, address, &chars, sizeof(chars));
        if (error) return error;
        t->chars = chars;
        /* Changing flow-control bytes must not strand a stopped writer. */
        resume(t); return 0;
    }
    case NX_TIOCFLUSH: flush(t); resume(t); return 0;
    case NX_TIOCGETD: {
        int32_t discipline = 0;
        return copyout(vm, address, &discipline, sizeof(discipline));
    }
    case NX_TIOCSETD: {
        int32_t discipline;
        int error = copyin(vm, address, &discipline, sizeof(discipline));
        return error ? error : discipline ? -NX_ENXIO : 0;
    }
    default: return -NX_ENOTTY;
    }
}
