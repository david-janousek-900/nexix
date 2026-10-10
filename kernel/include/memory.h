#ifndef NEXIX_MEMORY_H
#define NEXIX_MEMORY_H
#include "boot.h"
#define PAGE_SIZE 4096UL
#define PHYSICAL_LIMIT (1UL << 30)
/* Requires boot_parse success. Early, single-core init; interrupts disabled.
 * Reserve [0, reserved_end) and boot data. Never reset a live allocator. */
void page_allocator_init(const struct boot_info *info, uintptr_t reserved_end);
/* Physical addresses, identity-mapped for now. Zero signals exhaustion.
 * Pages are not cleared. All operations require caller serialization. */
uintptr_t page_alloc(void);
/* Reject unaligned, reserved, out-of-range, and already-free addresses. */
bool page_free(uintptr_t address);
uint32_t page_free_count(void);
#endif
