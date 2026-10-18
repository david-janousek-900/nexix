#ifndef NEXIX_DISK_H
#define NEXIX_DISK_H
#include "block.h"
#include <nexix/disk.h>
struct disk_binding {struct block_device *device;unsigned refs;};
struct disk_view {struct block_device device;struct disk_binding *binding;uint32_t first;unsigned retries;};
bool disk_major(unsigned,bool raw);
int disk_bind(unsigned kind,unsigned unit,struct block_device *);
bool disk_backing_bound(struct block_device *);
int disk_acquire(uint16_t dev,bool raw,struct disk_view *);
void disk_release(struct disk_view *);
/* Auxiliary ATA disks (primary slave, secondary master/slave), shared native
 * backing for each applicable historical family. IRQs disabled throughout. */
void disk_native_init(void);
#endif
