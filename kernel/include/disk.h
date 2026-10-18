#ifndef NEXIX_DISK_H
#define NEXIX_DISK_H
#include "block.h"
#include "disk_order.h"
#include <nexix/disk.h>
struct disk_binding {struct block_device *device;unsigned refs;struct disk_queue queue;};
struct disk_view {
    struct block_device device;
    struct disk_binding *binding,*other;
    uint32_t first;
    unsigned retries,kind,minor,members,pending;
};
bool disk_major(unsigned,bool raw);
int disk_bind(unsigned kind,unsigned unit,struct block_device *);
bool disk_backing_bound(struct block_device *);
int disk_acquire(uint16_t dev,bool raw,struct disk_view *);
int disk_release(struct disk_view *); /* EBUSY retains all pins while requests are queued. */
bool disk_read_only(const struct disk_view *);
bool disk_region(const struct disk_view *,unsigned,struct block_device **,uint32_t *,uint32_t *);
/* Zero-initialize caller-owned requests. Storage callbacks submit/drain one
 * request synchronously; batching keeps buffers/views alive through dispatch. */
int disk_submit(struct disk_view *,struct disk_request *,uint32_t,void *,bool write);
int disk_dispatch(struct disk_binding *);
/* Auxiliary ATA disks (primary slave, secondary master/slave), shared native
 * backing for each applicable historical family. IRQs disabled throughout. */
void disk_native_init(void);
#endif
