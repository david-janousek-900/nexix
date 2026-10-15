#ifndef NEXIX_PTRACE_H
#define NEXIX_PTRACE_H
#include "process.h"
int trace_request(int64_t request,int64_t pid,uintptr_t address,uint64_t data,
                  uintptr_t result);
/* Stop on the live user frame; return after the debugger continues us. */
void proc_trace_stop(void);
#endif
