#include "libc_test.h"
#include "files.h"
#include "interrupts.h"
#include "io.h"
#include "serial.h"
#include "segments.h"
#include "user_test.h"
#include "process.h"
#include "syscall.h"
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
    /* Channel 0, rate generator, approximately 100 Hz. Enable IRQ0 only. */
    uint16_t divisor = 1193182 / 100;
    outb(0x43, 0x34);
    outb(0x40, divisor & 0xff);
    outb(0x40, divisor >> 8);
    outb(0x21, 0xfe);
}
uint64_t timer_ticks(void) { return ticks; }
void interrupt_dispatch(struct interrupt_frame *frame)
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
        syscall_dispatch(frame);
        return;
    }
    if (frame->vector == 0x81) {
        if (process_test_gate(frame) || syscall_test_gate(frame) || libc_test_gate(frame) || files_test_gate(frame) || user_mode_test_gate(frame)) return;
        frame->rax = (uint64_t)-NX_ENOSYS;
        return;
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
        if (frame->vector == 32)
            ++ticks;
        if (frame->vector >= 40)
            outb(0xa0, 0x20);
        outb(0x20, 0x20);
        if (frame->vector == 32) proc_timer(frame);
        return;
    }
    panic("unexpected interrupt vector");
}
