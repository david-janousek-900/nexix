#include "segments.h"
#include "paging.h"
#include <stddef.h>

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp[3], reserved1, ist[7], reserved2;
    uint16_t reserved3, iomap;
} __attribute__((packed));
struct gdt_pointer { uint16_t limit; uint64_t address; } __attribute__((packed));
struct guarded_stack {
    uint8_t guard[PAGE_SIZE];
    uint8_t stack[ENTRY_STACK_SIZE];
};
_Static_assert(sizeof(struct tss64) == 104, "64-bit TSS size");
_Static_assert(offsetof(struct tss64, ist) == 36, "64-bit TSS IST offset");
_Static_assert(offsetof(struct tss64, iomap) == 102, "64-bit TSS I/O offset");
static uint64_t gdt[7] __attribute__((aligned(16)));
static struct tss64 tss;
static struct guarded_stack entry_stacks[2] __attribute__((aligned(PAGE_SIZE)));
static struct guarded_stack double_fault_stack __attribute__((aligned(PAGE_SIZE)));
static struct guarded_stack nmi_stack __attribute__((aligned(PAGE_SIZE)));
static uintptr_t current_bottom, current_top;
extern void arch_load_gdt(const struct gdt_pointer *pointer);

uintptr_t entry_stack_bottom(unsigned context)
{
    if (context >= 2) panic("invalid entry stack index");
    return (uintptr_t)entry_stacks[context].stack;
}
uintptr_t entry_stack_top(unsigned context) { return entry_stack_bottom(context) + ENTRY_STACK_SIZE; }

void kernel_stack_set(uintptr_t bottom, uintptr_t top)
{
    if (!bottom || bottom >= top || (top & 15) || top > PHYSICAL_LIMIT || top - bottom < PAGE_SIZE)
        panic("invalid interrupt entry stack");
    current_bottom = bottom;
    current_top = top;
    tss.rsp[0] = top;
}

void segments_init(void)
{
    gdt[0] = 0;
    gdt[1] = 0x00af9a000000ffffULL;
    gdt[2] = 0x00cf92000000ffffULL;
    gdt[3] = 0x00cff2000000ffffULL;
    gdt[4] = 0x00affa000000ffffULL;
    uintptr_t base = (uintptr_t)&tss;
    gdt[5] = (sizeof(tss) - 1) | ((base & 0xffffffULL) << 16) |
        (0x89ULL << 40) | ((base & 0xff000000ULL) << 32);
    gdt[6] = base >> 32;
    kernel_stack_set(entry_stack_bottom(0), entry_stack_top(0));
    tss.ist[0] = (uintptr_t)double_fault_stack.stack + ENTRY_STACK_SIZE;
    tss.ist[1] = (uintptr_t)nmi_stack.stack + ENTRY_STACK_SIZE;
    tss.iomap = sizeof(tss); /* No I/O bitmap: user port I/O is denied. */
    struct gdt_pointer pointer = { sizeof(gdt) - 1, (uintptr_t)gdt };
    arch_load_gdt(&pointer);
}

void entry_stacks_guard(void)
{
    for (unsigned i = 0; i < 2; ++i)
        if (!paging_guard_page((uintptr_t)entry_stacks[i].guard))
            panic("cannot guard interrupt entry stack");
    if (!paging_guard_page((uintptr_t)double_fault_stack.guard) ||
        !paging_guard_page((uintptr_t)nmi_stack.guard))
        panic("cannot guard IST stacks");
}

bool interrupt_stack_valid(const struct interrupt_frame *frame)
{
    uintptr_t bottom = current_bottom, top = current_top;
    if (frame->vector == 8) {
        bottom = (uintptr_t)double_fault_stack.stack;
        top = bottom + ENTRY_STACK_SIZE;
    } else if (frame->vector == 2) {
        bottom = (uintptr_t)nmi_stack.stack;
        top = bottom + ENTRY_STACK_SIZE;
    }
    uintptr_t address = (uintptr_t)frame;
    return address >= bottom && address <= top - sizeof(*frame);
}

void test_invalid_entry_stack(void)
{
    tss.rsp[0] = (uintptr_t)entry_stacks[0].guard + PAGE_SIZE / 2;
}
