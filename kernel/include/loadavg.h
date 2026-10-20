#ifndef NEXIX_LOADAVG_H
#define NEXIX_LOADAVG_H
#include "v7/core.h"
#include <nexix/kinfo.h>
/* Call once per elapsed second, with IRQs disabled. Fixed point avoids FPU use. */
void loadavg_sample(const struct proc *,size_t);
void loadavg_snapshot(struct nx_kinfo_load *);
#endif
