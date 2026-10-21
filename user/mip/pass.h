/* Streaming replacement for reader.c/fort.c's machine-sized pass records.
 * Callbacks borrow text/IR only for the duration of the call. A nonzero
 * callback result aborts consumption. No parser or target globals are shared.
 */
#ifndef NX_MIP_PASS_H
#define NX_MIP_PASS_H
#include "mip.h"
#include <stdio.h>
#define MIP_PASS_LIMIT (64UL*1024*1024)
#define MIP_PASS_TEXT_LIMIT 65536U
struct mip_pass_writer {FILE *file;size_t bytes;unsigned failed,ended;};
struct mip_pass_consumer {
    int (*text)(void *,const unsigned char *,size_t);
    /* bits==0 is an expression; otherwise this is a scalar initializer. */
    int (*tree)(void *,const struct mip_arena *,uint32_t root,unsigned bits);
};
int mip_pass_begin(struct mip_pass_writer *,FILE *);
int mip_pass_text(struct mip_pass_writer *,const void *,size_t);
int mip_pass_tree(struct mip_pass_writer *,const struct mip_arena *,uint32_t,unsigned bits);
int mip_pass_end(struct mip_pass_writer *);
/* Requires explicit END and physical EOF, rejects unknown records/ABI/version,
 * and validates a complete record before calling its consumer. */
int mip_pass_read(FILE *,const struct mip_pass_consumer *,void *);
#endif
