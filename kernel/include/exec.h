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
    uintptr_t entry, stack, heap;
    size_t size, text, data, bss;
    uint16_t mode, uid, gid; /* Staged executable inode metadata. */
};
/* IRQs disabled; caller owns staged image until commit. No live VM mutation. */
/* Reader must fill exactly count bytes or return negative errno. */
typedef int (*elf_reader)(void *context, size_t offset, void *buffer, size_t count);
int elf_load_reader(elf_reader, void *context, size_t length,
                    const struct exec_args *, struct exec_image *);
struct v7fs;
int elf_load_inode(struct v7fs *, v7_ino_t, const struct exec_args *, struct exec_image *);
int elf_load(const void *file, size_t length, const struct exec_args *, struct exec_image *);
void exec_image_release(struct exec_image *);
int proc_exec(uintptr_t path, uintptr_t argv, uintptr_t envp, struct interrupt_frame *);
void exec_selftest(void);
bool root_exec_test_gate(struct interrupt_frame *);
#endif
