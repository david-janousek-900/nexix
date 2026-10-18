#include "packet.h"
#include "libc_test.h"
#include "files.h"
#include "tty.h"
#include "interrupts.h"
#include "io.h"
#include "serial.h"
#include "segments.h"
#include "user_test.h"
#include "process.h"
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
_Noreturn void panic(const char *message)
{
    __asm__ volatile ("cli" : : : "memory");
    serial_puts("Nexix PANIC: ");
    serial_puts(message);
    serial_puts("\r\n");
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
    /* Channel 0, rate generator, approximately 100 Hz. Unmask IRQ0. */
    uint16_t divisor = 1193182 / 100;
    outb(0x43, 0x34);
    outb(0x40, divisor & 0xff);
    outb(0x40, divisor >> 8);
    outb(0x21, inb(0x21) & ~1U);
}
uint64_t timer_ticks(void) { return ticks; }
static void dispatch(struct interrupt_frame *frame)
{
    if (frame->vector == 8 || frame->vector == 2) {
        if (!interrupt_stack_valid(frame))
            panic("invalid IST entry stack");
        if (frame->vector == 8)
            serial_puts("NEXIX_DOUBLE_FAULT_STACK_OK\r\n");
    } else if ((frame->cs & 3) == 3) {
        if (proc_current()) proc_user_interrupt(frame);
        else user_mode_interrupt(frame);
    }
    if (frame->vector == 0x80) {
        bool stepping=(frame->rflags&0x100)!=0;
        syscall_dispatch(frame);
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
        serial_puts("Nexix: trap cs="); serial_hex(frame->cs);
        serial_puts(" ss="); serial_hex(frame->ss);
        serial_puts(" rsp="); serial_hex(frame->rsp);
        serial_puts(" frame="); serial_hex((uintptr_t)frame);
        serial_puts("\r\n");
        serial_puts("Nexix: exception vector="); serial_hex(frame->vector);
        serial_puts(" error="); serial_hex(frame->error);
        serial_puts(" rip="); serial_hex(frame->rip);
        if (frame->vector == 14) {
            uintptr_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            serial_puts(" cr2="); serial_hex(cr2);
        }
        serial_puts("\r\n");
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
        if (frame->vector == 32) {
            ++ticks;
            struct proc *owner=proc_current();
            if(!owner)++cpu_idle;
            else if((frame->cs&3)!=3)++cpu_system;
            else if(owner->p_nice>20)++cpu_nice;
            else ++cpu_user;
        }
        if (frame->vector == 36) serial_interrupt();
        if (frame->vector >= 40)
            outb(0xa0, 0x20);
        outb(0x20, 0x20);
        if (frame->vector == 32) {tty_timer(ticks);packet_timer(ticks);proc_timer(frame);}
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
}
