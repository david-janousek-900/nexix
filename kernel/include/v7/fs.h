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
    /* Optional resident-inode observer, invoked only after a successful metadata write. */
    void (*inode_changed)(struct v7fs *, const struct v7fs_inode *);
};
/* Kernel buffers/pathnames and decoded inode copies. Reference ownership
 * belongs to the files layer, not this storage API.
 * Caller serializes the entire operation (kernel: IRQs disabled), and never
 * mutates backing storage behind the mount/cache. Initialize fs to zero.
 * Writes are synchronous; a write-I/O failure poisons the mount until remount.
 * Partial transfers report done; no crash/journal transaction guarantees. */
int v7fs_mount(struct v7fs *, struct block_device *, bool read_only);
int v7fs_unmount(struct v7fs *);
int v7fs_inode_get(struct v7fs *, v7_ino_t, struct v7fs_inode *);
int v7fs_lookup(struct v7fs *, v7_ino_t cwd, const char *path, v7_ino_t *result);
int v7fs_read(struct v7fs *, v7_ino_t, int64_t offset, void *, size_t count, size_t *done);
/* Bounded record append publishes EOF only after all bytes succeed. */
int v7fs_append_record(struct v7fs *,v7_ino_t,const void *,size_t);
int v7fs_write(struct v7fs *, v7_ino_t, int64_t offset, const void *, size_t count, size_t *done);
/* Low-level allocation APIs: blocks must be exclusively owned by the caller.
 * No duplicate-free detection or full filesystem consistency scan. */
int v7fs_alloc_block(struct v7fs *, uint32_t *block);
int v7fs_free_block(struct v7fs *, uint32_t block);
bool v7fs_access(const struct v7fs_inode *, uint16_t uid, uint16_t gid, unsigned bits);
int v7fs_lookup_user(struct v7fs *, v7_ino_t, const char *, v7_ino_t *, uint16_t uid, uint16_t gid);
int v7fs_lookup_root_user(struct v7fs *, v7_ino_t cwd, v7_ino_t rootino, const char *, v7_ino_t *, uint16_t uid, uint16_t gid);
/* Update only mode, owner and timestamps, preserving block ownership. */
#define V7FS_ATIME 1U
#define V7FS_MTIME 2U
#define V7FS_CTIME 4U
int v7fs_touch(struct v7fs *, v7_ino_t, unsigned flags);
int v7fs_sync(struct v7fs *);
/* Metadata changes always set ctime to the system clock. */
int v7fs_metadata(struct v7fs *, const struct v7fs_inode *);
int v7fs_truncate(struct v7fs *, v7_ino_t);
int v7fs_release_inode(struct v7fs *, v7_ino_t); /* nlink zero, no live references */
/* Names are trusted, zero-padded/truncated V7 components. Caller checks
 * credentials; release_inode additionally requires exclusive ownership. */
int v7fs_create(struct v7fs *, v7_ino_t parent, const char name[V7_DIRSIZ], uint16_t mode, uint16_t uid, uint16_t gid, v7_ino_t *);
int v7fs_mknod(struct v7fs *, v7_ino_t, const char[V7_DIRSIZ], uint16_t mode, uint16_t uid, uint16_t gid, uint16_t device, v7_ino_t *);
int v7fs_link_privileged(struct v7fs *, v7_ino_t, v7_ino_t, const char[V7_DIRSIZ], bool);
int v7fs_unlink_privileged(struct v7fs *, v7_ino_t, const char[V7_DIRSIZ], v7_ino_t *, bool);
int v7fs_link(struct v7fs *, v7_ino_t ino, v7_ino_t parent, const char name[V7_DIRSIZ]);
int v7fs_unlink(struct v7fs *, v7_ino_t parent, const char name[V7_DIRSIZ], v7_ino_t *);
void filesystem_selftest(void);
#endif
