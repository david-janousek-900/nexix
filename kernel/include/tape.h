#ifndef NEXIX_TAPE_H
#define NEXIX_TAPE_H
#include "process.h"
#include <nexix/tape.h>
/* Native little-endian image: 64-byte checked header, then length/checksum,
 * even-padded payload, repeated length/checksum. Length zero is a tape mark. */
struct tape_media {
    void *context;
    uint64_t size;
    int (*read)(void *,uint64_t,void *,size_t);
    int (*write)(void *,uint64_t,const void *,size_t);
};
struct tape {
    struct tape_media media;
    uint64_t end,records,capacity,generation,position,record,residual;
    unsigned attempts;
    bool poisoned;
};
typedef int (*tape_copy)(void *,size_t,void *,size_t);
int tape_image(struct tape *,struct tape_media,unsigned attempts,bool initialize,uint64_t capacity);
int64_t tape_read_record(struct tape *,size_t,tape_copy,void *);
int64_t tape_write_record(struct tape *,size_t,tape_copy,void *);
int tape_length(struct tape *,uint32_t *);
int tape_move(struct tape *,bool backward);
int tape_rewind(struct tape *);
int tape_mark(struct tape *);
int tape_erase(struct tape *);
/* Kernel device/regular-file binding adapter; IRQ serialized. */
struct tape_unit;
bool tape_matches(uint16_t,bool raw);
int tape_open(uint16_t,bool raw,unsigned access,void **);
int tape_close(void *);
bool tape_seekable(void *);
int64_t tape_io(void *,struct address_space *,uintptr_t,size_t,bool,int64_t);
int tape_ioctl(void *,struct address_space *,uint64_t,uintptr_t,int64_t *);
bool tape_image_busy(const struct inode *);
#endif
