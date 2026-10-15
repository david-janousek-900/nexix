#ifndef NEXIX_PROFILE_H
#define NEXIX_PROFILE_H
#include "process.h"
/* Single CPU, IRQs disabled. Configuration is lazy: only sampled words are
 * accessed. Sampling faults disable profiling without changing syscall errno. */
int profile_configure(uintptr_t base,size_t size,uintptr_t offset,uint64_t scale);
void profile_sample(struct proc *,uintptr_t pc,uint64_t ticks);
#endif
