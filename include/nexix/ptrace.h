#ifndef NEXIX_ABI_PTRACE_H
#define NEXIX_ABI_PTRACE_H
#include <stddef.h>
#include <stdint.h>
#include <nexix/fpu.h>
#define NX_PT_TRACE_ME 0
#define NX_PT_READ_I 1
#define NX_PT_READ_D 2
#define NX_PT_READ_U 3
#define NX_PT_WRITE_I 4
#define NX_PT_WRITE_D 5
#define NX_PT_WRITE_U 6
#define NX_PT_CONTINUE 7
#define NX_PT_KILL 8
#define NX_PT_STEP 9
/* Request 3/6 addresses are byte offsets in this public snapshot, never
 * offsets in the private kernel user structure. Words are aligned uint64_t. */
struct nx_ptrace_user {
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8;
    uint64_t rdi,rsi,rbp,rdx,rcx,rbx,rax;
    uint64_t vector,error,rip,cs,rflags,rsp,ss;
    struct nx_fpu_state fpu;
};
#define NX_PT_OFFSET(field) offsetof(struct nx_ptrace_user,field)
_Static_assert(sizeof(struct nx_ptrace_user)==688,"ptrace snapshot ABI");
_Static_assert(NX_PT_OFFSET(fpu)==176,"ptrace FPU offset");
#endif
