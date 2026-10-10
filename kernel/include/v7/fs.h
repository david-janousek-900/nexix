#ifndef NEXIX_V7_FS_H
#define NEXIX_V7_FS_H
#include "block.h"
#include "v7/disk.h"
#include "v7/constants.h"
#define V7FS_CACHE 29
#define V7FS_MAX_FILE_BLOCKS (10UL + 128UL + 128UL * 128 + 128UL * 128 * 128)
#define V7FS_MAX_FILE_SIZE (V7FS_MAX_FILE_BLOCKS * V7_BSIZE)
struct v7fs_inode {
    v7_ino_t number;
    uint16_t mode, nlink, uid, gid;
    uint32_t size, addr[V7_NADDR], atime, mtime, ctime;
};
struct v7fs_cache { uint32_t block; bool valid; uint8_t bytes[V7_BSIZE]; };
struct v7fs {
    struct block_device *device;
    struct v7_disk_super super;
    uint32_t isize, fsize, inodes;
    bool mounted, read_only, failed;
    unsigned cursor;
    struct v7fs_cache cache[V7FS_CACHE];
};
/* Single mount, no inode reference ownership yet. Kernel buffers/pathnames.
 * Caller serializes the entire operation (kernel: IRQs disabled), and never
 * mutates backing storage behind the mount/cache. Initialize fs to zero.
 * Writes are synchronous; a write-I/O failure poisons the mount until remount.
 * Partial transfers report done; no crash/journal transaction guarantees. */
int v7fs_mount(struct v7fs *, struct block_device *, bool read_only);
int v7fs_unmount(struct v7fs *);
int v7fs_inode_get(struct v7fs *, v7_ino_t, struct v7fs_inode *);
int v7fs_lookup(struct v7fs *, v7_ino_t cwd, const char *path, v7_ino_t *result);
int v7fs_read(struct v7fs *, v7_ino_t, int64_t offset, void *, size_t count, size_t *done);
int v7fs_write(struct v7fs *, v7_ino_t, int64_t offset, const void *, size_t count, size_t *done);
/* Low-level allocation APIs: blocks must be exclusively owned by the caller.
 * No duplicate-free detection or full filesystem consistency scan. */
int v7fs_alloc_block(struct v7fs *, uint32_t *block);
int v7fs_free_block(struct v7fs *, uint32_t block);
void filesystem_selftest(void);
#endif
