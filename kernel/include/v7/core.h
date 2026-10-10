#ifndef NEXIX_V7_CORE_H
#define NEXIX_V7_CORE_H
/* Adapted from V7 h/{proc,user,inode,file,buf,text,param}.h.
 * See reference/README.md and the retained Caldera license. */
#include "v7/disk.h"
#include "v7/constants.h"
#include "paging.h"
#define V7_NPROC 150
#define V7_NOFILE 20
#define V7_NSIG 17
#define V7_NFILE 175
#define V7_NINODE 200
#define V7_NBUF 29
/* Matches arch/x86_64/context.S; all switches run with interrupts disabled. */
struct v7_context { uint64_t rbx, rbp, r12, r13, r14, r15, rsp, rip; };
struct user;
struct inode;
struct file;
struct text;
struct tty;
struct interrupt_frame;
enum v7_proc_state { V7_UNUSED, V7_SSLEEP, V7_SWAIT, V7_SRUN,
                     V7_SIDL, V7_SZOMB, V7_SSTOP };
enum { V7_SLOAD=01, V7_SSYS=02, V7_SLOCK=04, V7_SSWAP=010,
       V7_STRC=020, V7_SWTED=040, V7_SULOCK=0100 };
struct proc {
    uint8_t p_stat, p_flag, p_pri, p_time, p_cpu, p_nice;
    uint16_t p_sig;
    v7_uid_t p_uid;
    v7_pid_t p_pgrp, p_pid, p_ppid;
    size_t p_size; /* bytes, not PDP-11 clicks */
    void *p_wchan;
    struct text *p_textp;
    struct proc *p_link;
    v7_time_t p_clktim;
    /* Explicit zombie data replaces the historical xproc memory overlay. */
    int32_t p_xstat;
    v7_time_t p_utime, p_stime;
    struct address_space p_vm;
    uintptr_t p_stack_bottom, p_stack_top;
    struct v7_context p_context;
    struct user *p_user;
    void (*p_entry)(void *);
    void *p_argument;
    uint64_t p_deadline, p_preemptions;
    unsigned p_slice;
    bool p_timed_sleep, p_interruptible;
    uintptr_t p_brk_base, p_brk;
};
struct direct { v7_ino_t d_ino; char d_name[V7_DIRSIZ]; };
struct user {
    struct proc *u_procp;
    uint8_t u_segflg, u_error;
    v7_uid_t u_uid, u_gid, u_ruid, u_rgid;
    uintptr_t *u_ap;
    union { struct { int64_t r_val1, r_val2; }; v7_off_t r_off; v7_time_t r_time; } u_r;
    uintptr_t u_base, u_dirp; /* user addresses, never implicitly dereferenced */
    size_t u_count;
    v7_off_t u_offset;
    struct inode *u_cdir, *u_rdir, *u_pdir;
    char u_dbuf[V7_DIRSIZ];
    struct direct u_dent;
    struct file *u_ofile[V7_NOFILE];
    uint8_t u_pofile[V7_NOFILE];
    uintptr_t u_arg[6]; /* six full-width native syscall arguments */
    size_t u_tsize, u_dsize, u_ssize; /* bytes */
    uintptr_t u_signal[V7_NSIG];
    uintptr_t u_restorer[V7_NSIG];
    v7_time_t u_utime, u_stime, u_cutime, u_cstime;
    struct interrupt_frame *u_ar0;
    struct { uintptr_t pr_base, pr_off; size_t pr_size; uint32_t pr_scale; } u_prof;
    uint8_t u_intflg;
    struct tty *u_ttyp;
    v7_dev_t u_ttyd;
    struct { uintptr_t entry; size_t text, data, bss; } u_exdata;
    char u_comm[V7_DIRSIZ];
    v7_time_t u_start;
    uint8_t u_acflag;
    uint16_t u_cmask;
};
struct inode {
    uint8_t i_flag;
    uint32_t i_count;
    v7_dev_t i_dev;
    v7_ino_t i_number;
    uint16_t i_mode;
    int32_t i_nlink;
    v7_uid_t i_uid, i_gid;
    v7_off_t i_size;
    union {
        struct { v7_daddr_t i_addr[V7_NADDR], i_lastr; } i_f;
        v7_dev_t i_rdev;
    } i_un;
};
struct file {
    uint8_t f_flag;
    uint32_t f_count;
    struct inode *f_inode;
    v7_off_t f_offset; /* shared by dup/fork */
    uint32_t f_open_flags;
    void *f_private; /* in-memory pipe endpoint backing */
};
struct buf {
    uint32_t b_flags;
    struct buf *b_forw, *b_back, *av_forw, *av_back;
    v7_dev_t b_dev;
    size_t b_bcount;
    void *b_addr;
    v7_daddr_t b_blkno;
    uint8_t b_error;
    size_t b_resid; /* bytes, unlike V7's word count */
};
struct text {
    v7_daddr_t x_daddr;
    uintptr_t x_caddr;
    size_t x_size; /* bytes */
    struct inode *x_iptr;
    uint32_t x_count, x_ccount;
    uint8_t x_flag;
};
_Static_assert(offsetof(struct v7_context, rip) == 56, "context continuation offset");
_Static_assert(sizeof(struct v7_context) == 64, "context register layout");
_Static_assert(offsetof(struct v7_context, rsp) == 48, "context stack offset");
_Static_assert(sizeof(((struct user *)0)->u_arg[0]) == 8, "native syscall slot");
_Static_assert(sizeof(((struct user *)0)->u_signal[0]) == 8, "signal address width");
#endif
