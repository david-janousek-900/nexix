#ifndef NEXIX_EXEC_H
#define NEXIX_EXEC_H
#include "process.h"
#define EXEC_STACK_TOP (USER_LIMIT - PAGE_SIZE)
#define EXEC_STACK_PAGES 8
#define EXEC_ARG_BYTES 8192
#define EXEC_ARG_COUNT 64
struct exec_args {
    char bytes[EXEC_ARG_BYTES];
    size_t used, argc, envc;
    size_t argv[EXEC_ARG_COUNT], envp[EXEC_ARG_COUNT];
};
struct exec_image {
    struct address_space vm;
    uintptr_t entry, stack;
    size_t size, text, data, bss;
};
/* IRQs disabled; caller owns staged image until commit. No live VM mutation. */
int elf_load(const void *file, size_t length, const struct exec_args *, struct exec_image *);
void exec_image_release(struct exec_image *);
int proc_exec(uintptr_t path, uintptr_t argv, uintptr_t envp, struct interrupt_frame *);
void exec_selftest(void);
#endif
