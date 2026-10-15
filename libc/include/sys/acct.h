#ifndef NEXIX_LIBC_SYS_ACCT_H
#define NEXIX_LIBC_SYS_ACCT_H
#include <sys/types.h>
#include <nexix/acct.h>
typedef unsigned short comp_t;
#define AFORK NX_AFORK
#define ASU NX_ASU
/* Source contract for sa/accton; native time_t and explicit zero padding. */
struct acct {
    char ac_comm[10];
    comp_t ac_utime,ac_stime,ac_etime;
    time_t ac_btime;
    uid_t ac_uid;
    gid_t ac_gid;
    short ac_mem;
    comp_t ac_io;
    dev_t ac_tty;
    unsigned char ac_flag,ac_reserved[5];
};
_Static_assert(sizeof(struct acct)==40,"native accounting layout");
int acct(const char *file);
#endif
