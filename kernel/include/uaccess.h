#ifndef NEXIX_UACCESS_H
#define NEXIX_UACCESS_H
#include "paging.h"
/* Managed, live address spaces only; kernel buffer pointers/capacities trusted.
 * Helpers serialize lookup and access by disabling IRQs for the whole operation.
 * Zero-length copies touch nothing and accept any user address.
 * copyin/out preflight the full range and make no changes on EFAULT.
 * copyinstr stops at NUL, includes NUL in copied, and may leave a partial result
 * on EFAULT/E2BIG. Kernel callers must check the return value before use. */
bool user_access_ok(const struct address_space *, uintptr_t address, size_t length, bool write);
int copyin(const struct address_space *, uintptr_t source, void *destination, size_t length);
int copyout(const struct address_space *, uintptr_t destination, const void *source, size_t length);
int copyinstr(const struct address_space *, uintptr_t source, char *destination,
              size_t capacity, size_t *copied);
#endif
