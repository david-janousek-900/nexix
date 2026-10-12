#ifndef NEXIX_ABI_CORE_H
#define NEXIX_ABI_CORE_H
#include <stdint.h>
#include <stddef.h>
/* ELF64 envelope; vendor notes avoid imposing a foreign Linux register ABI. */
#define NX_CORE_VERSION 1
#define NX_CORE_INFO 1
#define NX_CORE_REGISTERS 2
#define NX_CORE_FPU 3
#define NX_CORE_MAX_LOADS 64
struct nx_core_elf_header {
    uint8_t ident[16];uint16_t type,machine;uint32_t version;
    uint64_t entry,phoff,shoff;uint32_t flags;
    uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;
};
struct nx_core_program {
    uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;
};
struct nx_core_note { uint32_t namesz,descsz,type; };
struct nx_core_info {
    uint32_t version,signal;int32_t pid,ppid;
    uint16_t ruid,euid,rgid,egid;int64_t time;
    char command[16];
};
/* Same user-register order as the native interrupt/signal frame, no pointers
 * to kernel process/user/file structures. FPU note is 512-byte FXSAVE64. */
struct nx_core_registers {
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rdx,rcx,rbx,rax;
    uint64_t vector,error,rip,cs,rflags,rsp,ss;
};
_Static_assert(sizeof(struct nx_core_elf_header)==64,"core ELF header");
_Static_assert(sizeof(struct nx_core_program)==56,"core program header");
_Static_assert(sizeof(struct nx_core_note)==12,"core note header");
_Static_assert(sizeof(struct nx_core_info)==48,"core process note");
_Static_assert(sizeof(struct nx_core_registers)==176,"core register note");
#endif
