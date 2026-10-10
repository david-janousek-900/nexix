#ifndef NEXIX_SEGMENTS_H
#define NEXIX_SEGMENTS_H
#include "interrupts.h"
#include <stdbool.h>
#include <stdint.h>
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS 0x1b
#define USER_CS 0x23
#define TSS_SELECTOR 0x28
#define ENTRY_STACK_SIZE (16UL * 1024)

void segments_init(void);
void entry_stacks_guard(void); /* Call after managed paging is active. */
void kernel_stack_set(uintptr_t bottom, uintptr_t top);
bool interrupt_stack_valid(const struct interrupt_frame *frame);
uintptr_t entry_stack_bottom(unsigned context);
uintptr_t entry_stack_top(unsigned context);
void test_invalid_entry_stack(void); /* Deliberate RSP0 fault for the IST test. */
/* Enter with fixed user selectors, IF=1, IOPL=0, and cleared GPRs except RDI.
 * Returns only through the bootstrap test handler; not a process scheduler. */
uint64_t arch_user_test_enter(uintptr_t entry, uintptr_t stack, uint64_t argument);
_Noreturn void arch_user_test_return(uint64_t result);
#endif
