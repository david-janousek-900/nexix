#ifndef NEXIX_IDE_H
#define NEXIX_IDE_H
#include "block.h"
/* Legacy ATA channels, polling PIO, 512-byte sectors, LBA28.
 * Unit 0/1 primary master/slave; 2/3 secondary master/slave. */
int ide_init(struct block_device *device);
int ide_init_unit(struct block_device *,unsigned unit);
#endif
