#ifndef NEXIX_ABI_SIGNAL_H
#define NEXIX_ABI_SIGNAL_H
#define NX_SIGHUP 1
#define NX_SIGINT 2
#define NX_SIGQUIT 3
#define NX_SIGILL 4
#define NX_SIGTRAP 5
#define NX_SIGABRT 6
#define NX_SIGEMT 7
#define NX_SIGFPE 8
#define NX_SIGKILL 9
#define NX_SIGBUS 10
#define NX_SIGSEGV 11
#define NX_SIGSYS 12
#define NX_SIGPIPE 13
#define NX_SIGALRM 14
#define NX_SIGTERM 15
#define NX_NSIG 17
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <nexix/fpu.h>
/* Saved interrupt frame following the handler's return address on its stack. */
struct nx_signal_context {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags, rsp, ss;
    struct nx_fpu_state fpu;
};
_Static_assert(sizeof(struct nx_signal_context) == 688, "native signal context");
#endif
#endif
