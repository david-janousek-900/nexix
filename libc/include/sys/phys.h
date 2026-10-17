#ifndef NEXIX_LIBC_SYS_PHYS_H
#define NEXIX_LIBC_SYS_PHYS_H
#include <sys/types.h>
#include <nexix/phys.h>
/* Native adaptation: slot 0..7, 0..2 pages, page-aligned physical byte address. */
int phys(unsigned slot,size_t pages,uintptr_t physical);
#endif
