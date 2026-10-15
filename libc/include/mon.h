#ifndef NEXIX_LIBC_MON_H
#define NEXIX_LIBC_MON_H
#include <nexix/mon.h>
/* Start: monitor(low, high, unsigned-short buffer, size_t words, size_t calls).
 * Stop/output: monitor(NULL). The optional sizes must have native size_t type. */
int monitor(void *lowpc,...);
void mcount(void);
extern void *countbase;
#endif
