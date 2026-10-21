#ifndef NEXIX_INTERRUPTS_H
#define NEXIX_INTERRUPTS_H
#include <stdbool.h>
#include <stdint.h>
/* Matches interrupts.S and the long-mode hardware frame for either CPL. */
struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags, rsp, ss;
};
void interrupts_init(void);
void timer_init(void);
uint64_t timer_ticks(void);
/* Device IRQ lines 5..13 (not the cascade or IDE 14/15) may each be attached
 * once. The handler runs in IRQ context before EOI: it must acknowledge its
 * device (level-triggered PCI INTx) and must not sleep. Attach unmasks the
 * line; detach masks it and waits for nothing (single CPU, IRQs disabled). */
bool irq_attach(unsigned irq, void (*handler)(void *), void *argument);
void irq_detach(unsigned irq);
void interrupt_dispatch(struct interrupt_frame *frame);
_Noreturn void panic(const char *message);
#endif
