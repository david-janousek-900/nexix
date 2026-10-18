#ifndef NEXIX_STORAGE_H
#define NEXIX_STORAGE_H
#include "block.h"
#include "disk.h"
#define STORAGE_SLOTS 8
/* IRQs disabled. Backing lifetime extends through the final close. */
struct storage_handle { struct block_device *device; unsigned refs; bool mounted; bool controller; uint16_t dev; struct disk_view view; };
bool storage_block_device(uint16_t);
bool storage_raw_device(uint16_t);
int storage_bind(unsigned minor, struct block_device *, bool mounted);
int storage_open(uint16_t dev, bool raw, bool write, struct storage_handle **);
int storage_mount_open(uint16_t dev, bool read_only, struct storage_handle **);
void storage_mount_close(struct storage_handle *);
void storage_close(struct storage_handle *);
int storage_transfer(struct storage_handle *, uint64_t offset, void *, size_t, bool write, size_t *done);
#endif
