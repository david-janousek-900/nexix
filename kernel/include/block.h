#ifndef NEXIX_BLOCK_H
#define NEXIX_BLOCK_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#define BLOCK_SIZE 512U
/* Synchronous, kernel buffers only. Caller serializes device/backing lifetime.
 * Callbacks transfer exactly one sector or return negative Nexix errno. */
struct block_device {
    uint32_t blocks;
    bool read_only;
    void *context;
    int (*read)(struct block_device *, uint32_t, void *);
    int (*write)(struct block_device *, uint32_t, const void *);
};
struct ramdisk { struct block_device device; uint8_t *bytes; size_t length; };
int ramdisk_init(struct ramdisk *, void *backing, size_t length, bool read_only);
int block_read(struct block_device *, uint32_t first, uint32_t count, void *buffer);
int block_write(struct block_device *, uint32_t first, uint32_t count, const void *buffer);
#endif
