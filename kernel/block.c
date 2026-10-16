#include "block.h"
#include <nexix/kinfo.h>
static uint64_t read_sectors,write_sectors,read_errors,write_errors;
void block_statistics(struct nx_kinfo_io *r)
{r->read_sectors=read_sectors;r->write_sectors=write_sectors;r->read_errors=read_errors;r->write_errors=write_errors;}
#include <nexix/errno.h>
static int ram_read(struct block_device *dev, uint32_t block, void *buffer)
{
    struct ramdisk *ram = dev->context;
    const uint8_t *src = ram->bytes + (size_t)block * BLOCK_SIZE;
    uint8_t *dst = buffer;
    for (size_t i = 0; i < BLOCK_SIZE; ++i) dst[i] = src[i];
    return 0;
}
static int ram_write(struct block_device *dev, uint32_t block, const void *buffer)
{
    struct ramdisk *ram = dev->context;
    uint8_t *dst = ram->bytes + (size_t)block * BLOCK_SIZE;
    const uint8_t *src = buffer;
    for (size_t i = 0; i < BLOCK_SIZE; ++i) dst[i] = src[i];
    return 0;
}
int ramdisk_init(struct ramdisk *ram, void *backing, size_t length, bool read_only)
{
    if (!ram || !backing || !length || length % BLOCK_SIZE || length / BLOCK_SIZE > UINT32_MAX)
        return -NX_EINVAL;
    *ram = (struct ramdisk){.bytes = backing, .length = length,
        .device = {.blocks = length / BLOCK_SIZE, .read_only = read_only,
            .context = ram, .read = ram_read, .write = ram_write}};
    return 0;
}
static int valid(struct block_device *dev, uint32_t first, uint32_t count, const void *buffer)
{
    if (!dev || !dev->blocks || !dev->read) return -NX_ENODEV;
    if (first > dev->blocks || count > dev->blocks - first) return -NX_EIO;
    if (count && !buffer) return -NX_EINVAL;
    return 0;
}
int block_read(struct block_device *dev, uint32_t first, uint32_t count, void *buffer)
{
    int error = valid(dev, first, count, buffer);
    if (error) return error;
    uint8_t *dst = buffer;
    for (uint32_t i = 0; i < count; ++i) {
        error = dev->read(dev, first + i, dst + (size_t)i * BLOCK_SIZE);
        if (error) { ++read_errors; return error; }
        ++read_sectors;
    }
    return 0;
}
int block_write(struct block_device *dev, uint32_t first, uint32_t count, const void *buffer)
{
    int error = valid(dev, first, count, buffer);
    if (error) return error;
    if (dev->read_only) return -NX_EROFS;
    if (!dev->write) return -NX_ENODEV;
    const uint8_t *src = buffer;
    for (uint32_t i = 0; i < count; ++i) {
        error = dev->write(dev, first + i, src + (size_t)i * BLOCK_SIZE);
        if (error) { ++write_errors; return error; }
        ++write_sectors;
    }
    return 0;
}
