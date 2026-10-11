#ifndef NEXIX_IDE_H
#define NEXIX_IDE_H
#include "block.h"
/* Primary master, polling PIO, 512-byte sectors, LBA28. */
int ide_init(struct block_device *device);
#endif
