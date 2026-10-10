#ifndef NEXIX_FILES_H
#define NEXIX_FILES_H
#include "process.h"
#include "v7/fs.h"
#include <nexix/file.h>
/* IRQs disabled; one root filesystem, kernel-owned tables. */
int files_mount(struct v7fs *); /* NULL detaches, EBUSY if references remain */
bool files_refs_valid(const struct user *);
void files_fork(struct user *);
void files_exit(struct user *);
void files_exec(struct user *);
unsigned files_live(void);
int64_t files_syscall(uint64_t number, const uintptr_t args[6]);
void files_selftest(void);
bool files_test_gate(struct interrupt_frame *frame);
#endif
