#ifndef NEXIX_DECTAPE_H
#define NEXIX_DECTAPE_H
#include "block.h"
#include <nexix/dectape.h>
/* Serialized native replacement of TC-11 queue/position/interrupt I/O. */
bool tc_major(unsigned);
int tc_bind(unsigned,struct block_device *);
int tc_acquire(uint16_t,struct block_device **);
void tc_release(struct block_device *);
struct block_device *tc_backing(struct block_device *);
bool tc_bound(struct block_device *);
/* Regular-file media loading, called only after tapectl's root check. */
struct inode;
int tc_media_load(const struct nx_tape_load *,bool unload);
bool tc_image_busy(const struct inode *);
#endif
