#ifndef NEXIX_SCHEDULER_H
#define NEXIX_SCHEDULER_H
#include "v7/core.h"
#define V7_PUSER 50
#define V7_NZERO 20
void scheduler_priority(struct proc *p);
void scheduler_tick(struct proc *table,size_t count,struct proc *running,bool second);
struct proc *scheduler_pick(struct proc *table,size_t count,unsigned cursor);
#endif
