/* Native adaptation of V7 alloc.c, subr.c bmap, iget.c, rdwri.c and nami.c.
 * Original source and license are retained under reference/. */
#include "v7/fs.h"
#include <nexix/errno.h>
static void copy(void *to, const void *from, size_t n)
{
    uint8_t *d = to; const uint8_t *s = from;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
}
static void clear(void *to, size_t n)
{ uint8_t *p = to; for (size_t i = 0; i < n; ++i) p[i] = 0; }
static int ready(struct v7fs *fs, bool write)
{
    if (!fs || !fs->mounted) return -NX_ENODEV;
    if (fs->failed) return -NX_EIO;
    if (write && fs->read_only) return -NX_EROFS;
    return 0;
}
static bool data_block(struct v7fs *fs, uint32_t block)
{ return block >= fs->isize && block < fs->fsize; }
static int read_block(struct v7fs *fs, uint32_t block, void *buffer)
{
    if (block >= fs->fsize) return -NX_EIO;
    for (unsigned i = 0; i < V7FS_CACHE; ++i)
        if (fs->cache[i].valid && fs->cache[i].block == block) {
            copy(buffer, fs->cache[i].bytes, V7_BSIZE); return 0;
        }
    struct v7fs_cache *entry = &fs->cache[fs->cursor++ % V7FS_CACHE];
    entry->valid = false;
    int error = block_read(fs->device, block, 1, entry->bytes);
    if (error) return error;
    entry->block = block; entry->valid = true;
    copy(buffer, entry->bytes, V7_BSIZE);
    return 0;
}
static int write_block(struct v7fs *fs, uint32_t block, const void *buffer)
{
    if (block >= fs->fsize) return -NX_EIO;
    int error = block_write(fs->device, block, 1, buffer);
    if (error) { fs->failed = true; return error; }
    for (unsigned i = 0; i < V7FS_CACHE; ++i)
        if (fs->cache[i].valid && fs->cache[i].block == block)
            copy(fs->cache[i].bytes, buffer, V7_BSIZE);
    return 0;
}
int v7fs_inode_get(struct v7fs *fs, v7_ino_t number, struct v7fs_inode *inode)
{
    int error = ready(fs, false);
    if (error) return error;
    if (!inode || !number || number > fs->inodes) return -NX_EINVAL;
    uint8_t bytes[V7_BSIZE];
    error = read_block(fs, v7_itod(number), bytes);
    if (error) return error;
    const struct v7_disk_inode *disk = (const void *)(bytes + v7_itoo(number) * 64);
    struct v7fs_inode value = {.number = number, .mode = v7_disk_u16(disk->mode),
        .nlink = v7_disk_u16(disk->nlink), .uid = v7_disk_u16(disk->uid),
        .gid = v7_disk_u16(disk->gid), .size = v7_disk_u32(disk->size),
        .atime = v7_disk_u32(disk->atime), .mtime = v7_disk_u32(disk->mtime),
        .ctime = v7_disk_u32(disk->ctime)};
    if (!value.mode) return -NX_ENOENT;
    unsigned type = value.mode & V7_IFMT;
    if (type == V7_IFREG || type == V7_IFDIR) {
        if (value.size > V7FS_MAX_FILE_SIZE || (type == V7_IFDIR && value.size % 16)) return -NX_EIO;
        for (unsigned i = 0; i < V7_NADDR; ++i) {
            value.addr[i] = v7_disk_addr(disk->addr + 3 * i);
            if (value.addr[i] && !data_block(fs, value.addr[i])) return -NX_EIO;
        }
    } else if (type == V7_IFCHR || type == V7_IFBLK) {
        value.addr[0] = v7_disk_addr(disk->addr); /* Encoded device number. */
    } else return -NX_EIO;
    *inode = value;
    return 0;
}
static int inode_put(struct v7fs *fs, const struct v7fs_inode *inode)
{
    uint8_t bytes[V7_BSIZE];
    int error = read_block(fs, v7_itod(inode->number), bytes);
    if (error) return error;
    struct v7_disk_inode *disk = (void *)(bytes + v7_itoo(inode->number) * 64);
    v7_disk_put16(disk->mode, inode->mode); v7_disk_put16(disk->nlink, inode->nlink);
    v7_disk_put16(disk->uid, inode->uid); v7_disk_put16(disk->gid, inode->gid);
    v7_disk_put32(disk->size, inode->size);
    for (unsigned i = 0; i < V7_NADDR; ++i)
        if (!v7_disk_putaddr(disk->addr + i * 3, inode->addr[i])) return -NX_EIO;
    return write_block(fs, v7_itod(inode->number), bytes);
}
static int free_stack(struct v7fs *fs, unsigned count, const uint8_t *entries)
{
    if (!count || count > 50) return -NX_EIO;
    for (unsigned i = 0; i < count; ++i) {
        uint32_t block = v7_disk_u32(entries + i * 4);
        if (!block) { if (i != 0) return -NX_EIO; continue; }
        if (!data_block(fs, block)) return -NX_EIO;
        for (unsigned j = 0; j < i; ++j)
            if (block == v7_disk_u32(entries + j * 4)) return -NX_EIO;
    }
    return 0;
}
int v7fs_mount(struct v7fs *fs, struct block_device *device, bool read_only)
{
    if (!fs) return -NX_EINVAL;
    if (fs->mounted) return -NX_EBUSY;
    if (!device || device->blocks < 4) return -NX_ENODEV;
    if (!read_only && device->read_only) return -NX_EROFS;
    if (!read_only && !device->write) return -NX_ENODEV;
    clear(fs, sizeof(*fs));
    int error = block_read(device, V7_SUPERB, 1, &fs->super);
    if (error) return error;
    fs->isize = v7_disk_u16(fs->super.isize); fs->fsize = v7_disk_u32(fs->super.fsize);
    if (fs->isize < 3 || fs->isize >= fs->fsize || fs->fsize > device->blocks ||
        fs->fsize > 0x1000000 || (fs->isize - 2) * 8 > UINT16_MAX ||
        v7_disk_u16(fs->super.nfree) > 50 || v7_disk_u16(fs->super.ninode) > 100)
        return -NX_EIO;
    fs->inodes = (fs->isize - 2) * 8;
    unsigned nfree = v7_disk_u16(fs->super.nfree);
    if (nfree && (error = free_stack(fs, nfree, (const uint8_t *)fs->super.free))) return error;
    for (unsigned i = 0; i < v7_disk_u16(fs->super.ninode); ++i) {
        unsigned ino = v7_disk_u16(fs->super.inode[i]);
        if (!ino || ino > fs->inodes) return -NX_EIO;
    }
    fs->device = device; fs->read_only = read_only; fs->mounted = true;
    struct v7fs_inode root;
    error = v7fs_inode_get(fs, V7_ROOTINO, &root);
    if (error == -NX_ENOENT || error == -NX_EINVAL) error = -NX_EIO;
    if (!error && ((root.mode & V7_IFMT) != V7_IFDIR || root.size < 32)) error = -NX_EIO;
    if (error) { fs->mounted = false; fs->device = NULL; return error; }
    return 0;
}
int v7fs_unmount(struct v7fs *fs)
{
    if (!fs || !fs->mounted) return -NX_ENODEV;
    clear(fs, sizeof(*fs)); /* All writes already reached the synchronous device. */
    return 0;
}
int v7fs_alloc_block(struct v7fs *fs, uint32_t *result)
{
    int error = ready(fs, true);
    if (error) return error;
    if (!result) return -NX_EINVAL;
    struct v7_disk_super next = fs->super;
    unsigned count = v7_disk_u16(next.nfree);
    if (!count) return -NX_ENOSPC;
    error = free_stack(fs, count, (const uint8_t *)next.free);
    if (error) return error;
    uint32_t block = v7_disk_u32(next.free[--count]);
    if (!block) return -NX_ENOSPC;
    if (!count) {
        uint8_t chain[V7_BSIZE];
        error = read_block(fs, block, chain);
        if (error) return error;
        count = v7_disk_u16(chain);
        error = free_stack(fs, count, chain + 2);
        if (error) return error;
        for (unsigned i = 0; i < count; ++i) {
            uint32_t entry = v7_disk_u32(chain + 2 + 4 * i);
            if (entry == block) return -NX_EIO;
            v7_disk_put32(next.free[i], entry);
        }
    }
    v7_disk_put16(next.nfree, count);
    uint32_t total = v7_disk_u32(next.tfree);
    if (total) v7_disk_put32(next.tfree, total - 1);
    next.fmod = 0;
    /* Remove from free list before clearing old chain contents. */
    error = write_block(fs, V7_SUPERB, &next);
    if (error) return error;
    fs->super = next;
    uint8_t zero[V7_BSIZE] = {0};
    error = write_block(fs, block, zero);
    if (error) return error;
    *result = block;
    return 0;
}
int v7fs_free_block(struct v7fs *fs, uint32_t block)
{
    int error = ready(fs, true);
    if (error) return error;
    if (!data_block(fs, block)) return -NX_EINVAL;
    struct v7_disk_super next = fs->super;
    unsigned count = v7_disk_u16(next.nfree);
    if (count > 50) return -NX_EIO;
    if (!count) { count = 1; v7_disk_put32(next.free[0], 0); }
    for (unsigned i = 0; i < count; ++i)
        if (v7_disk_u32(next.free[i]) == block) return -NX_EINVAL;
    if (count == 50) {
        uint8_t chain[V7_BSIZE] = {0};
        v7_disk_put16(chain, count);
        for (unsigned i = 0; i < count; ++i) v7_disk_put32(chain + 2 + 4 * i, v7_disk_u32(next.free[i]));
        error = write_block(fs, block, chain);
        if (error) return error;
        count = 0;
    }
    v7_disk_put32(next.free[count++], block); v7_disk_put16(next.nfree, count);
    uint32_t total = v7_disk_u32(next.tfree);
    if (total < fs->fsize - fs->isize) v7_disk_put32(next.tfree, total + 1);
    next.fmod = 0;
    error = write_block(fs, V7_SUPERB, &next);
    if (!error) fs->super = next;
    return error;
}
/* Direct plus single/double/triple indirection, following V7 subr.c bmap. */
static int bmap(struct v7fs *fs, struct v7fs_inode *inode, uint32_t logical, bool allocate, uint32_t *result)
{
    if (logical >= V7FS_MAX_FILE_BLOCKS) return -NX_EFBIG;
    unsigned slot, depth = 0;
    uint32_t index = logical, span = 1;
    if (logical < 10) slot = logical;
    else {
        index -= 10;
        for (depth = 1; depth <= 3; ++depth) {
            span *= V7_NINDIR;
            if (index < span) break;
            index -= span;
        }
        slot = 9 + depth;
    }
    uint32_t block = inode->addr[slot];
    if (!block && allocate) {
        int error = v7fs_alloc_block(fs, &block);
        if (error) return error;
        inode->addr[slot] = block;
        error = inode_put(fs, inode);
        if (error) {
            inode->addr[slot] = 0;
            if (!fs->failed) (void)v7fs_free_block(fs, block);
            return error;
        }
    }
    while (depth && block) {
        if (!data_block(fs, block)) return -NX_EIO;
        uint8_t pointers[V7_BSIZE];
        int error = read_block(fs, block, pointers);
        if (error) return error;
        span /= V7_NINDIR;
        unsigned entry = index / span;
        index %= span;
        uint32_t next = v7_disk_u32(pointers + entry * 4);
        if (!next && allocate) {
            error = v7fs_alloc_block(fs, &next);
            if (error) return error;
            v7_disk_put32(pointers + entry * 4, next);
            error = write_block(fs, block, pointers);
            if (error) return error;
        }
        block = next; --depth;
    }
    if (block && !data_block(fs, block)) return -NX_EIO;
    *result = block;
    return 0;
}
int v7fs_read(struct v7fs *fs, v7_ino_t number, int64_t offset, void *buffer, size_t count, size_t *done)
{
    if (!done) return -NX_EINVAL;
    *done = 0;
    int error = ready(fs, false);
    if (error) return error;
    if (offset < 0 || (count && !buffer)) return -NX_EINVAL;
    struct v7fs_inode inode;
    error = v7fs_inode_get(fs, number, &inode);
    if (error) return error;
    unsigned type = inode.mode & V7_IFMT;
    if (type != V7_IFREG && type != V7_IFDIR) return -NX_ENODEV;
    if ((uint64_t)offset >= inode.size) return 0;
    if (count > inode.size - (uint64_t)offset) count = inode.size - offset;
    uint8_t bytes[V7_BSIZE];
    while (*done < count) {
        uint32_t block;
        error = bmap(fs, &inode, offset / V7_BSIZE, false, &block);
        if (error) return error;
        if (block) { error = read_block(fs, block, bytes); if (error) return error; }
        else clear(bytes, sizeof(bytes));
        size_t within = offset % V7_BSIZE, n = V7_BSIZE - within;
        if (n > count - *done) n = count - *done;
        copy((uint8_t *)buffer + *done, bytes + within, n);
        *done += n; offset += n;
    }
    return 0;
}
static int write_file(struct v7fs *fs, v7_ino_t number, int64_t offset, const void *buffer, size_t count, size_t *done, bool directory)
{
    if (!done) return -NX_EINVAL;
    *done = 0;
    int error = ready(fs, true);
    if (error) return error;
    if (offset < 0 || (count && !buffer)) return -NX_EINVAL;
    if ((uint64_t)offset > V7FS_MAX_FILE_SIZE || count > V7FS_MAX_FILE_SIZE - offset) return -NX_EFBIG;
    struct v7fs_inode inode;
    error = v7fs_inode_get(fs, number, &inode);
    if (error) return error;
    if ((inode.mode & V7_IFMT) == V7_IFDIR && !directory) return -NX_EISDIR;
    if ((inode.mode & V7_IFMT) != V7_IFREG && !directory) return -NX_ENODEV;
    uint8_t bytes[V7_BSIZE];
    if (count && (uint64_t)offset > inode.size && inode.size % V7_BSIZE) {
        uint32_t tail;
        error = bmap(fs, &inode, inode.size / V7_BSIZE, false, &tail);
        if (error) return error;
        if (tail) {
            error = read_block(fs, tail, bytes);
            if (error) return error;
            for (unsigned i = inode.size % V7_BSIZE; i < V7_BSIZE; ++i) bytes[i] = 0;
            error = write_block(fs, tail, bytes);
            if (error) return error;
        }
    }
    while (*done < count) {
        uint32_t block;
        error = bmap(fs, &inode, offset / V7_BSIZE, true, &block);
        if (error) return error;
        error = read_block(fs, block, bytes);
        if (error) return error;
        size_t within = offset % V7_BSIZE, n = V7_BSIZE - within;
        if (n > count - *done) n = count - *done;
        copy(bytes + within, (const uint8_t *)buffer + *done, n);
        error = write_block(fs, block, bytes);
        if (error) return error;
        offset += n; *done += n;
        if ((uint64_t)offset > inode.size) {
            inode.size = offset;
            error = inode_put(fs, &inode);
            if (error) return error;
        }
    }
    return 0;
}
static int lookup(struct v7fs *fs, v7_ino_t cwd, const char *path, v7_ino_t *result, uint16_t uid, uint16_t gid, bool check)
{
    int error = ready(fs, false);
    if (error) return error;
    if (!path || !result || !*path) return -NX_ENOENT;
    v7_ino_t current = *path == '/' ? V7_ROOTINO : cwd;
    struct v7fs_inode inode;
    error = v7fs_inode_get(fs, current, &inode);
    if (error) return error;
    while (*path) {
        bool slash = false;
        while (*path == '/') { ++path; slash = true; }
        if (slash && (inode.mode & V7_IFMT) != V7_IFDIR) return -NX_ENOTDIR;
        if (!*path) break;
        if ((inode.mode & V7_IFMT) != V7_IFDIR) return -NX_ENOTDIR;
        if (check && !v7fs_access(&inode, uid, gid, 1)) return -NX_EACCES;
        char name[V7_DIRSIZ] = {0};
        unsigned n = 0;
        /* Preserve V7's truncation of long components to the first 14 bytes. */
        while (*path && *path != '/') { if (n < V7_DIRSIZ) name[n++] = *path; ++path; }
        struct v7_disk_direct entry;
        bool found = false;
        for (uint32_t offset = 0; offset < inode.size; offset += sizeof(entry)) {
            size_t done;
            error = v7fs_read(fs, current, offset, &entry, sizeof(entry), &done);
            if (error) return error;
            if (done != sizeof(entry)) return -NX_EIO;
            unsigned i = 0;
            while (i < V7_DIRSIZ && name[i] == entry.name[i]) ++i;
            if (i == V7_DIRSIZ && v7_disk_u16(entry.ino)) {
                current = v7_disk_u16(entry.ino); found = true; break;
            }
        }
        if (!found) return -NX_ENOENT;
        error = v7fs_inode_get(fs, current, &inode);
        if (error) return error == -NX_EINVAL ? -NX_EIO : error;
    }
    *result = current;
    return 0;
}

int v7fs_write(struct v7fs *fs, v7_ino_t ino, int64_t off, const void *buf, size_t n, size_t *done)
{ return write_file(fs, ino, off, buf, n, done, false); }
bool v7fs_access(const struct v7fs_inode *inode, uint16_t uid, uint16_t gid, unsigned bits)
{
    if (!uid) return true;
    unsigned shift = uid == inode->uid ? 6 : gid == inode->gid ? 3 : 0;
    return ((inode->mode >> shift) & bits) == bits;
}
int v7fs_lookup(struct v7fs *fs, v7_ino_t cwd, const char *path, v7_ino_t *ino)
{ return lookup(fs, cwd, path, ino, 0, 0, false); }
int v7fs_lookup_user(struct v7fs *fs, v7_ino_t cwd, const char *path, v7_ino_t *ino, uint16_t uid, uint16_t gid)
{ return lookup(fs, cwd, path, ino, uid, gid, true); }
/* Zero-length truncation detaches roots before reclaiming their owned trees.
 * Write errors poison the mount; no crash-atomic metadata transaction. */
static int release_tree(struct v7fs *fs, uint32_t block, unsigned depth)
{
    if (!block) return 0;
    if (!data_block(fs, block)) return -NX_EIO;
    if (depth) {
        uint8_t bytes[V7_BSIZE];
        int error = read_block(fs, block, bytes);
        if (error) return error;
        for (unsigned i = 0; i < V7_NINDIR; ++i) {
            error = release_tree(fs, v7_disk_u32(bytes + 4 * i), depth - 1);
            if (error) return error;
        }
    }
    return v7fs_free_block(fs, block);
}
int v7fs_truncate(struct v7fs *fs, v7_ino_t ino)
{
    int error = ready(fs, true);
    if (error) return error;
    struct v7fs_inode inode;
    error = v7fs_inode_get(fs, ino, &inode);
    if (error) return error;
    if ((inode.mode & V7_IFMT) != V7_IFREG) return -NX_EISDIR;
    uint32_t roots[V7_NADDR];
    copy(roots, inode.addr, sizeof(roots));
    clear(inode.addr, sizeof(inode.addr)); inode.size = 0;
    error = inode_put(fs, &inode);
    if (error) return error;
    for (unsigned i = 0; i < V7_NADDR; ++i) {
        error = release_tree(fs, roots[i], i < 10 ? 0 : i - 9);
        if (error) { fs->failed = true; return error; }
    }
    return 0;
}
int v7fs_release_inode(struct v7fs *fs, v7_ino_t ino)
{
    struct v7fs_inode inode;
    int error = v7fs_inode_get(fs, ino, &inode);
    if (error) return error;
    if (inode.nlink) return -NX_EBUSY;
    error = v7fs_truncate(fs, ino);
    if (error) return error;
    error = v7fs_inode_get(fs, ino, &inode);
    if (error) return error;
    inode.mode = 0;
    error = inode_put(fs, &inode);
    if (error) return error;
    unsigned total = v7_disk_u16(fs->super.tinode);
    if (total < fs->inodes) v7_disk_put16(fs->super.tinode, total + 1);
    v7_disk_put16(fs->super.ninode, 0); /* Future allocations rescan the i-list. */
    return write_block(fs, V7_SUPERB, &fs->super);
}
static int directory_slot(struct v7fs *fs, v7_ino_t parent, const char name[V7_DIRSIZ],
                          uint32_t *offset, v7_ino_t *number)
{
    struct v7fs_inode dir;
    int error = v7fs_inode_get(fs, parent, &dir);
    if (error) return error;
    if ((dir.mode & V7_IFMT) != V7_IFDIR) return -NX_ENOTDIR;
    *offset = dir.size; *number = 0;
    for (uint32_t pos = 0; pos < dir.size; pos += 16) {
        struct v7_disk_direct entry; size_t done;
        error = v7fs_read(fs, parent, pos, &entry, sizeof(entry), &done);
        if (error) return error;
        if (done != sizeof(entry)) return -NX_EIO;
        v7_ino_t ino = v7_disk_u16(entry.ino);
        if (!ino) { if (*offset == dir.size) *offset = pos; continue; }
        unsigned i = 0; while (i < V7_DIRSIZ && name[i] == entry.name[i]) ++i;
        if (i == V7_DIRSIZ) { *offset = pos; *number = ino; return 0; }
    }
    return 0;
}
static int insert(struct v7fs *fs, v7_ino_t parent, uint32_t pos, const char name[V7_DIRSIZ], v7_ino_t ino)
{
    struct v7_disk_direct entry = {0}; size_t done;
    v7_disk_put16(entry.ino, ino); copy(entry.name, name, V7_DIRSIZ);
    int error = write_file(fs, parent, pos, &entry, sizeof(entry), &done, true);
    return error ? error : done == sizeof(entry) ? 0 : -NX_EIO;
}
static bool reserved_name(const char name[V7_DIRSIZ])
{ return !name[0] || (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))); }
int v7fs_create(struct v7fs *fs, v7_ino_t parent, const char name[V7_DIRSIZ],
                uint16_t mode, uint16_t uid, uint16_t gid, v7_ino_t *out)
{
    int error = ready(fs, true);
    if (error) return error;
    if (reserved_name(name) || !out) return -NX_EINVAL;
    uint32_t pos; v7_ino_t existing;
    error = directory_slot(fs, parent, name, &pos, &existing);
    if (error) return error;
    if (existing) return -NX_EEXIST;
    uint8_t bytes[V7_BSIZE];
    for (uint32_t number = 3; number <= fs->inodes; ++number) {
        error = read_block(fs, v7_itod(number), bytes);
        if (error) return error;
        struct v7_disk_inode *disk = (void *)(bytes + v7_itoo(number) * 64);
        if (v7_disk_u16(disk->mode)) continue;
        clear(disk, sizeof(*disk)); v7_disk_put16(disk->mode, V7_IFREG | (mode & 07777));
        v7_disk_put16(disk->nlink, 1); v7_disk_put16(disk->uid, uid); v7_disk_put16(disk->gid, gid);
        error = write_block(fs, v7_itod(number), bytes);
        if (error) return error;
        unsigned total = v7_disk_u16(fs->super.tinode);
        if (total) v7_disk_put16(fs->super.tinode, total - 1);
        v7_disk_put16(fs->super.ninode, 0);
        error = write_block(fs, V7_SUPERB, &fs->super);
        if (!error) error = insert(fs, parent, pos, name, number);
        if (error) {
            if (!fs->failed) {
                struct v7fs_inode rollback;
                if (!v7fs_inode_get(fs, number, &rollback)) {
                    rollback.nlink = 0;
                    if (!inode_put(fs, &rollback)) (void)v7fs_release_inode(fs, number);
                }
            }
            return error;
        }
        *out = number; return 0;
    }
    return -NX_ENOSPC;
}
int v7fs_link(struct v7fs *fs, v7_ino_t ino, v7_ino_t parent, const char name[V7_DIRSIZ])
{
    int error = ready(fs, true);
    if (error) return error;
    if (reserved_name(name)) return -NX_EPERM;
    struct v7fs_inode inode;
    error = v7fs_inode_get(fs, ino, &inode);
    if (error) return error;
    if ((inode.mode & V7_IFMT) != V7_IFREG) return -NX_EPERM;
    if (!inode.nlink || inode.nlink >= 32767) return -NX_EMLINK;
    uint32_t pos; v7_ino_t existing;
    error = directory_slot(fs, parent, name, &pos, &existing);
    if (error) return error;
    if (existing) return -NX_EEXIST;
    ++inode.nlink; error = inode_put(fs, &inode);
    if (!error) error = insert(fs, parent, pos, name, ino);
    if (error && !fs->failed) { --inode.nlink; (void)inode_put(fs, &inode); }
    return error;
}
int v7fs_unlink(struct v7fs *fs, v7_ino_t parent, const char name[V7_DIRSIZ], v7_ino_t *out)
{
    int error = ready(fs, true);
    if (error) return error;
    if (reserved_name(name) || !out) return -NX_EPERM;
    uint32_t pos; v7_ino_t ino;
    error = directory_slot(fs, parent, name, &pos, &ino);
    if (error) return error;
    if (!ino) return -NX_ENOENT;
    struct v7fs_inode inode;
    error = v7fs_inode_get(fs, ino, &inode);
    if (error) return error;
    if ((inode.mode & V7_IFMT) != V7_IFREG) return -NX_EPERM;
    if (!inode.nlink) return -NX_EIO;
    error = insert(fs, parent, pos, name, 0);
    if (error) return error;
    --inode.nlink; error = inode_put(fs, &inode);
    if (!error) *out = ino;
    return error;
}
