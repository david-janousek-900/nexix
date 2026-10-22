#ifndef NEXIX_FILES_H
#define NEXIX_FILES_H
#include "process.h"
#include "v7/fs.h"
#include <nexix/file.h>
/* IRQs disabled; device-qualified inodes, bounded mounts and owned tables. */
struct exec_args;
struct exec_image;
int files_mount_root(struct v7fs *); /* Production mount enables filesystem exec. */
bool files_exec_enabled(void);
int files_image_text(v7_ino_t,struct exec_image *);
int files_load_exec(const char *, const struct exec_args *, struct exec_image *);
int files_mount(struct v7fs *); /* NULL detaches, EBUSY if references remain */
bool files_refs_valid(const struct user *);
void files_fork(struct user *);
void files_exit(struct user *);
void files_exec(struct user *);
unsigned files_live(void);
void files_inode_changed(struct v7fs *,const struct v7fs_inode *);
int files_tape_backing(uintptr_t,bool,struct inode **);
int files_tape_read(struct inode *,uint64_t,void *,size_t);
int files_tape_write(struct inode *,uint64_t,const void *,size_t);
int files_account_open(uintptr_t,struct inode **);
int files_account_append(struct inode *,const void *,size_t);
int files_core_begin(struct inode **);
int files_core_write(struct inode *,uint64_t,const void *,size_t);
bool files_volume_busy(v7_dev_t);
int files_reset_volumes(void); /* Quiescent kernel teardown. */
int files_mountpoint_put(struct inode *); /* Preflight then non-I/O pin release. */
int files_inode_put(struct inode *);
int64_t files_syscall(uint64_t number, const uintptr_t args[6]);
/* Socket descriptors (task 425). reserve reports EMFILE/ENFILE without
 * allocating; install takes ownership of the socket on success only. */
int files_socket_reserve(struct user *);
int files_socket_install(struct user *, void *socket);
void *files_socket(struct user *, int64_t fd, int *error);   /* EBADF, ENOTSOCK */
void files_selftest(void);
bool files_test_gate(struct interrupt_frame *frame);
#endif
