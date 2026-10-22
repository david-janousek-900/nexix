#include "console.h"
#include "packet.h"
#include "keyboard.h"
#include "modem.h"
#include "libc_test.h"
#include "files.h"
#include "tty.h"
#include "interrupts.h"
#include "tick_clock.h"
#include "io.h"
#include "serial.h"
#include "segments.h"
#include "user_test.h"
#include "process.h"
#include "scheduler.h"
#include "callout.h"
#include "profile.h"
#include <nexix/kinfo.h>
static uint64_t cpu_user,cpu_nice,cpu_system,cpu_idle;
void cpu_statistics(struct nx_kinfo_io *r)
{r->user_ticks=cpu_user;r->nice_ticks=cpu_nice;r->system_ticks=cpu_system;r->idle_ticks=cpu_idle;}
#include "signals.h"
#include "syscall.h"
#include "exec.h"
#include <nexix/errno.h>
#include <stddef.h>
struct idt_gate {
    uint16_t offset_low, selector;
    uint8_t ist, attributes;
    uint16_t offset_mid;
    uint32_t offset_high, reserved;
} __attribute__((packed));
struct idt_pointer {
    uint16_t limit;
    uint64_t address;
} __attribute__((packed));
_Static_assert(sizeof(struct idt_gate) == 16, "IDT gate layout");
_Static_assert(offsetof(struct interrupt_frame, vector) == 120, "ISR frame layout");
_Static_assert(offsetof(struct interrupt_frame, cs) == 144, "ISR CS offset");
extern const uintptr_t isr_table[256];
static struct idt_gate idt[256] __attribute__((aligned(16)));
static volatile uint64_t ticks;
static struct tick_clock tick_clock;
static uint64_t cycles(void)
{
    uint32_t lo,hi;
    __asm__ volatile("lfence; rdtsc":"=a"(lo),"=d"(hi)::"memory");
    return ((uint64_t)hi<<32)|lo;
}
_Noreturn void panic(const char *message)
{
    __asm__ volatile ("cli" : : : "memory");
    console_puts("PANIC: ");
    console_puts(message);
    console_puts("\r\n");
    for (;;)
        __asm__ volatile ("hlt");
}
void interrupts_init(void)
{
    __asm__ volatile ("cli" : : : "memory");
    for (unsigned i = 0; i < 256; ++i) {
        uintptr_t address = isr_table[i];
        idt[i] = (struct idt_gate) {
            .offset_low = address,
            .selector = 8,
            .attributes = 0x8e,
            .ist = i == 8 ? 1 : (i == 2 ? 2 : 0),
            .offset_mid = address >> 16,
            .offset_high = address >> 32,
        };
    }
    idt[0x80].attributes = 0xee; /* Native syscall ABI. */
    idt[0x81].attributes = 0xee; /* Bootstrap test gate; hooks check active state. */
    idt[3].attributes = 0xee; /* User INT3 software breakpoints. */
    struct idt_pointer pointer = { sizeof(idt) - 1, (uintptr_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(pointer) : "memory");
    /* Remap legacy PICs to vectors 32..47 and mask all IRQs. */
    outb(0x20, 0x11); io_wait();
    outb(0xa0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xa1, 0x28); io_wait();
    outb(0x21, 4); io_wait();
    outb(0xa1, 2); io_wait();
    outb(0x21, 1); io_wait();
    outb(0xa1, 1); io_wait();
    outb(0x21, 0xff);
    outb(0xa1, 0xff);
}
void timer_init(void)
{
    /* Calibrate TSC with PIT channel 2 while IRQs remain masked. Channel 0
     * is only a wakeup source: elapsed time must survive coalesced IRQs. */
    uint16_t divisor = 1193182 / 100;
    uint8_t speaker=inb(0x61);
    outb(0x61,speaker&~3U);outb(0x43,0xb0);
    outb(0x42,divisor&255);outb(0x42,divisor>>8);
    uint64_t start=cycles();outb(0x61,(speaker&~2U)|1);
    unsigned polls=0;
    while(!(inb(0x61)&0x20))if(++polls==10000000U)panic("PIT calibration timeout");
    tick_clock=(struct tick_clock){.last=cycles()};
    tick_clock.period=tick_clock.last-start;
    outb(0x61,speaker);
    if(!tick_clock.period)panic("invalid TSC calibration");
    /* Channel 0, rate generator, approximately 100 Hz. Unmask IRQ0. */
    outb(0x43, 0x34);
    outb(0x40, divisor & 0xff);
    outb(0x40, divisor >> 8);
    outb(0x21, inb(0x21) & ~1U);
}
uint64_t timer_ticks(void) { return ticks; }
static struct { void (*handler)(void *); void *argument; } irq_handlers[16];
static void irq_mask_set(unsigned irq, bool masked)
{
    uint16_t port = irq < 8 ? 0x21 : 0xa1;
    uint8_t bit = 1U << (irq & 7), value = inb(port);
    outb(port, masked ? value | bit : value & ~bit);
    if (irq >= 8 && !masked) outb(0x21, inb(0x21) & ~4U);   /* cascade */
}
bool irq_attach(unsigned irq, void (*handler)(void *), void *argument)
{
    if (irq < 5 || irq >= 14 || !handler) return false;
    uintptr_t flags = proc_irq_save();
    bool ok = !irq_handlers[irq].handler;
    if (ok) {
        irq_handlers[irq].handler = handler;
        irq_handlers[irq].argument = argument;
        irq_mask_set(irq, false);
    }
    proc_irq_restore(flags);
    return ok;
}
void irq_detach(unsigned irq)
{
    if (irq < 5 || irq >= 14) return;
    uintptr_t flags = proc_irq_save();
    irq_mask_set(irq, true);
    irq_handlers[irq].handler = NULL;
    irq_handlers[irq].argument = NULL;
    proc_irq_restore(flags);
}
uint64_t timer_deadline(unsigned milliseconds)
{
    if(!tick_clock.period)return 0; /* Calibrated period is 10 ms. */
    return cycles()+tick_clock.period*((milliseconds+9)/10);
}
bool timer_expired(uint64_t deadline) { return (int64_t)(cycles()-deadline)>=0; }
/* All callers hold IRQ serialization; callbacks may schedule and resume.
 * Elapsed periods are shared, so a catch-up preempted by proc_timer cannot
 * absorb ticks another context adds while it is switched out. */
static uint64_t pending_ticks;
static void timer_advance_to(struct interrupt_frame *frame,uint64_t now)
{
    if(!tick_clock.period)return;
    pending_ticks+=tick_clock_advance(&tick_clock,now);
    while(pending_ticks){
        --pending_ticks;++ticks;
        struct proc *owner=proc_current();
        if(!owner)++cpu_idle;
        else if((frame->cs&3)!=3)++cpu_system;
        else if(owner->p_nice>20)++cpu_nice;
        else ++cpu_user;
        callout_tick(ticks);tty_timer(ticks);modem_timer(ticks);packet_timer(ticks);proc_timer(frame);
    }
}
void timer_poll(struct interrupt_frame *frame)
{timer_advance_to(frame,cycles());}
static void syscall_timer(struct interrupt_frame *frame,uint64_t now,bool kernel)
{
    struct interrupt_frame sample=*frame;
    if(kernel){sample.cs=8;sample.vector=32;}
    timer_advance_to(&sample,now);
}
static void dispatch(struct interrupt_frame *frame)
{
    if (frame->vector == 8 || frame->vector == 2) {
        if (!interrupt_stack_valid(frame))
            panic("invalid IST entry stack");
        if (frame->vector == 2) {
            /* NMI runs on IST and must not enter the scheduler or console.
             * Pulse the PC parity/channel-check latch reset bits when set.
             * Software-injected NMIs have no legacy latch to acknowledge. */
            uint8_t status=inb(0x61);
            uint8_t reset=((status&0x80)?4:0)|((status&0x40)?8:0);
            if(reset){outb(0x61,status|reset);outb(0x61,status&~reset);}
            return;
        }
        if (frame->vector == 8)
            console_puts("DOUBLE FAULT STACK OK\r\n");
    } else if ((frame->cs & 3) == 3) {
        if (proc_current()) proc_user_interrupt(frame);
        else user_mode_interrupt(frame);
    }
    if (frame->vector == 0x80) {
        bool stepping=(frame->rflags&0x100)!=0;
        /* Drain preceding user time before entering the interrupt-masked
         * syscall; drain elapsed kernel time before IRET changes CPL again. */
        syscall_timer(frame,cycles(),false);
        syscall_dispatch(frame);
        syscall_timer(frame,cycles(),true);
        /* INT clears hardware TF on entry. Report the completed user syscall
         * before another user instruction, never single-step kernel code.
         * Exec supplies its own trap; sigreturn may independently restore TF. */
        if(stepping&&(frame->rflags&0x100)){
            frame->rflags&=~0x100UL;signal_post(proc_current(),NX_SIGTRAP);
        }
        signal_deliver(frame);
        return;
    }
    if (frame->vector == 0x81) {
        if (process_test_gate(frame) || syscall_test_gate(frame) || libc_test_gate(frame) || files_test_gate(frame) || tty_test_gate(frame) || root_exec_test_gate(frame) || user_mode_test_gate(frame)) { signal_deliver(frame); return; }
        frame->rax = (uint64_t)-NX_ENOSYS;
        return;
    }
    if (frame->vector < 32 && frame->vector != 2 && frame->vector != 8 &&
        (frame->cs & 3) == 3 && proc_current()) {
        unsigned n = frame->vector == 0 || frame->vector == 16 || frame->vector == 19 ? NX_SIGFPE :
            frame->vector == 6 ? NX_SIGILL : frame->vector == 1 || frame->vector == 3 ? NX_SIGTRAP : NX_SIGSEGV;
        if(n==NX_SIGTRAP)frame->rflags&=~0x100UL; /* V7 clears TBIT on trace/bpt. */
        /* Faults would repeat forever if ignored. Debug/breakpoint traps have
         * completed their instruction and may safely resume at the next PC. */
        if (n!=NX_SIGTRAP && !(proc_current()->p_flag&V7_STRC) && proc_current()->p_user->u_signal[n] == 1) proc_exit(n);
        signal_post(proc_current(), n); signal_deliver(frame); return;
    }
    if (frame->vector < 32) {
        console_puts("trap cs="); console_hex(frame->cs);
        console_puts(" ss="); console_hex(frame->ss);
        console_puts(" rsp="); console_hex(frame->rsp);
        console_puts(" frame="); console_hex((uintptr_t)frame);
        console_puts("\r\n");
        console_puts("exception vector="); console_hex(frame->vector);
        console_puts(" error="); console_hex(frame->error);
        console_puts(" rip="); console_hex(frame->rip);
        if (frame->vector == 14) {
            uintptr_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            console_puts(" cr2="); console_hex(cr2);
        }
        console_puts("\r\n");
        panic("unhandled CPU exception");
    }
    if (frame->vector >= 32 && frame->vector < 48) {
        /* Spurious IRQ7 needs no EOI; spurious IRQ15 needs master EOI only. */
        if (frame->vector == 39 || frame->vector == 47) {
            uint16_t pic = frame->vector == 39 ? 0x20 : 0xa0;
            outb(pic, 0x0b);
            if (!(inb(pic) & 0x80)) {
                if (frame->vector == 47)
                    outb(0x20, 0x20);
                return;
            }
        }
        if (frame->vector == 33) keyboard_interrupt();
        if (frame->vector == 35) serial_secondary_interrupt();
        if (frame->vector == 36) serial_interrupt();
        unsigned irq = frame->vector - 32;
        if (irq_handlers[irq].handler) irq_handlers[irq].handler(irq_handlers[irq].argument);
        if (frame->vector >= 40)
            outb(0xa0, 0x20);
        outb(0x20, 0x20);
        if (frame->vector == 32) timer_poll(frame);
        signal_deliver(frame);
        return;
    }
    panic("unexpected interrupt vector");
}
void interrupt_dispatch(struct interrupt_frame *frame)
{
    struct proc *owner=(frame->cs&3)==3?proc_current():NULL;
    bool trap=frame->vector<32||frame->vector==0x80||frame->vector==0x81;
    v7_time_t system=owner?owner->p_user->u_stime:0;
    dispatch(frame);
    /* V7 trap.c charges only this process's system ticks, never elapsed sleep,
     * another process's CPU time, or a kernel PC. Signals/exec may change RIP. */
    if(owner&&trap&&proc_current()==owner&&(frame->cs&3)==3&&owner->p_user->u_stime>system)
        profile_sample(owner,frame->rip,(uint64_t)owner->p_user->u_stime-(uint64_t)system);
    if(proc_current() && (frame->cs&3)==3)scheduler_priority(proc_current());
}
