#ifndef NEXIX_PHYS_H
#define NEXIX_PHYS_H
#include "process.h"
#include <nexix/phys.h>
_Static_assert(NX_PHYS_PAGE_SIZE==PAGE_SIZE,"physical page ABI");
/* IRQs disabled. Borrowed leaves hold references to managed physical mappings. */
int phys_request(uint64_t slot,uint64_t pages,uintptr_t physical);
void phys_release(uintptr_t physical);
bool phys_clone_leaf(struct address_space *,uintptr_t va,uintptr_t pa);
int phys_exec(const struct address_space *,struct address_space *,size_t *bytes);
#endif
