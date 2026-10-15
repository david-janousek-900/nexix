#ifndef NEXIX_ABI_ACCT_H
#define NEXIX_ABI_ACCT_H
#include <stdint.h>
#include <stddef.h>
#define NX_AFORK 01
#define NX_ASU 02
#define NX_ACCT_SIZE 40
/* Native little-endian record; CPU times in NX_CLK_TCK, elapsed in seconds. */
struct nx_acct {
    char comm[10];
    uint16_t utime,stime,etime;
    int64_t btime;
    uint16_t uid,gid;
    int16_t mem;
    uint16_t io,tty;
    uint8_t flag,reserved[5];
};
_Static_assert(sizeof(struct nx_acct)==NX_ACCT_SIZE,"accounting record size");
_Static_assert(offsetof(struct nx_acct,btime)==16,"accounting start time");
_Static_assert(offsetof(struct nx_acct,flag)==34,"accounting flags");
#endif
