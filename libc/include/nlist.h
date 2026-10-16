#ifndef NEXIX_LIBC_NLIST_H
#define NEXIX_LIBC_NLIST_H
#include <stdint.h>
/* Native names are strings, not the PDP-11 eight-byte inline field. */
struct nlist {
    const char *n_name;
    int n_type;
    uint64_t n_value;
};
#define N_UNDF 0
#define N_ABS 01
#define N_TEXT 02
#define N_DATA 03
#define N_BSS 04
#define N_TYPE 037
#define N_REG 024
#define N_FN 037
#define N_EXT 040
int nlist(const char *filename, struct nlist *list);
#endif
