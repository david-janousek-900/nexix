#ifndef NEXIX_LIBC_SYS_PTRACE_H
#define NEXIX_LIBC_SYS_PTRACE_H
#include <sys/types.h>
#include <nexix/ptrace.h>
long ptrace(int request,pid_t pid,void *address,long data);
#endif
