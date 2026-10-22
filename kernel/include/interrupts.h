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
/* TSC deadline for polled waits; 0 before timer_init calibrates the TSC. */
uint64_t timer_deadline(unsigned milliseconds);
bool timer_expired(uint64_t deadline);
void timer_poll(struct interrupt_frame *);
void interrupt_dispatch(struct interrupt_frame *frame);
_Noreturn void panic(const char *message);
#endif
