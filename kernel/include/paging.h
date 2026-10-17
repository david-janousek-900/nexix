#ifndef NEXIX_PAGING_H
#define NEXIX_PAGING_H
#include "memory.h"

#define USER_BASE PHYSICAL_LIMIT
#define USER_LIMIT (1UL << 47)
#define VM_WRITE 1U
#define VM_EXEC 2U
#define VM_USER 4U
#define VM_PHYS 8U /* Borrowed physical mapping; never free as a private leaf. */
struct address_space { uintptr_t root; };

/* Kernel-only callers; all operations currently require interrupts disabled.
 * Spaces own page tables, but mapped data frames remain caller-owned.
 * Initialize spaces to zero. Never copy or manually edit a live root. */
void paging_init(void);
bool paging_guard_page(uintptr_t address); /* Permanently unmap a reserved BSS page. */
const struct address_space *paging_kernel_space(void);
bool vm_create(struct address_space *space);
bool vm_destroy(struct address_space *space); /* Reject active/kernel spaces. */
void vm_activate(const struct address_space *space);
/* User mappings only, aligned addresses, allocated physical frames below 1 GiB.
 * Permissions are VM_WRITE and/or VM_EXEC; write+execute is rejected.
 * Existing mappings cannot be overwritten. Failed maps release new tables. */
bool vm_map(struct address_space *space, uintptr_t virtual_address,
            uintptr_t physical_address, unsigned permissions);
/* Trusted physical-mapping manager only: approved RAM or VGA frames, RW/NX.
 * rebind requires an existing physical leaf and never allocates tables. */
bool vm_map_physical(struct address_space *,uintptr_t va,uintptr_t pa);
bool vm_rebind_physical(struct address_space *,uintptr_t va,uintptr_t pa);
bool vm_unmap(struct address_space *space, uintptr_t virtual_address);
bool vm_protect(struct address_space *space, uintptr_t virtual_address, unsigned permissions);
/* Lookup also supports the shared kernel region; returns effective permissions. */
bool vm_translate(const struct address_space *space, uintptr_t virtual_address,
                  uintptr_t *physical_address, unsigned *permissions);
/* Enumerate private user leaves in ascending address order, skipping shared
 * kernel tables. IRQs disabled; callback must not mutate the traversed tables.
 * Returns false on an invalid space or when the callback stops traversal. */
bool vm_walk_user(const struct address_space *space,
                  bool (*visit)(uintptr_t, uintptr_t, unsigned, void *), void *argument);
void paging_selftest(bool exhaustion);
#endif
