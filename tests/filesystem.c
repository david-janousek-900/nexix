#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "v7/fs.h"
#include <nexix/errno.h>
static int64_t now = 1709210096;
int64_t clock_now(void) { return now; }
static struct v7fs fs;
static struct ramdisk ram;
static uint8_t *image, *original;
static size_t length;
struct fault_device {
    struct block_device device;
    struct block_device *backing;
    uint32_t bad_read, bad_write;
    unsigned reads, writes;
};
static int fault_read(struct block_device *d, uint32_t block, void *buffer)
{
    struct fault_device *fault = d->context;
    ++fault->reads;
    if (block == fault->bad_read) return -NX_EIO;
    return block_read(fault->backing, block, 1, buffer);
}
static int fault_write(struct block_device *d, uint32_t block, const void *buffer)
{
    struct fault_device *fault = d->context;
    ++fault->writes;
    if (block == fault->bad_write) return -NX_EIO;
    return block_write(fault->backing, block, 1, buffer);
}
static void reset(void)
{
    if (fs.mounted) assert(v7fs_unmount(&fs) == 0);
    memcpy(image, original, length);
    assert(ramdisk_init(&ram, image, length, false) == 0);
}
static struct v7_disk_super *super(void) { return (void *)(image + 512); }
static struct v7_disk_inode *disk_inode(unsigned ino) { return (void *)(image + 1024 + (ino - 1) * 64); }
static void bad_mount(void)
{
    assert(v7fs_mount(&fs, &ram.device, false) == -NX_EIO && !fs.mounted);
    reset();
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size == 1024 * 512 && fseek(file, 0, SEEK_SET) == 0);
    length = size;
    original = malloc(length); image = malloc(length);
    assert(original && image && fread(original, 1, length, file) == length);
    assert(fclose(file) == 0);
    reset();
    /* Byte-level fixtures independently pin PDP-11 word order and inode slot. */
    assert(image[512] == 10 && image[513] == 0);
    assert(image[514] == 0 && image[515] == 0 && image[516] == 0 && image[517] == 4);
    assert(image[1088] == 0xed && image[1089] == 0x41);
    assert(image[1090] == 5 && image[1091] == 0);
    uint8_t buf[2048], old[512];
    memset(buf, 0xa5, sizeof(buf));
    assert(ramdisk_init(NULL, image, length, false) == -NX_EINVAL);
    assert(ramdisk_init(&ram, image, length - 1, false) == -NX_EINVAL);
    assert(ramdisk_init(&ram, image, SIZE_MAX & ~(size_t)511, false) == -NX_EINVAL);
    assert(block_read(NULL, 0, 1, buf) == -NX_ENODEV);
    assert(block_read(&ram.device, UINT32_MAX, 2, buf) == -NX_EIO);
    for (unsigned i = 0; i < sizeof(buf); ++i) assert(buf[i] == 0xa5);
    memcpy(old, image + length - 512, 512);
    assert(block_write(&ram.device, 1023, 2, buf) == -NX_EIO);
    assert(!memcmp(old, image + length - 512, 512));
    ram.device.read_only = true;
    assert(block_write(&ram.device, 1023, 1, buf) == -NX_EROFS);
    assert(v7fs_mount(&fs, &ram.device, false) == -NX_EROFS);
    assert(v7fs_mount(&fs, &ram.device, true) == 0);
    size_t done = 123;
    assert(v7fs_write(&fs, 8, 0, buf, 1, &done) == -NX_EROFS && done == 0);
    assert(!memcmp(original, image, length));
    reset();
    v7_disk_put16(super()->isize, 2); bad_mount();
    v7_disk_put16(super()->isize, 9000); bad_mount();
    v7_disk_put32(super()->fsize, 1025); bad_mount();
    v7_disk_put16(super()->nfree, 51); bad_mount();
    v7_disk_put16(super()->ninode, 101); bad_mount();
    v7_disk_put32(super()->free[0], 1); bad_mount();
    v7_disk_put16(super()->inode[0], 65); bad_mount();
    v7_disk_put16(disk_inode(2)->mode, V7_IFREG | 0755); bad_mount();
    v7_disk_put16(disk_inode(2)->mode, 0); bad_mount();
    v7_disk_put32(disk_inode(2)->size, 33); bad_mount();
    assert(v7_disk_putaddr(disk_inode(2)->addr, 1)); bad_mount();
    assert(v7_disk_putaddr(disk_inode(5)->addr, 1024));
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    assert(v7fs_read(&fs, 5, 0, buf, 1, &done) == -NX_EIO && done == 0);
    reset();
    /* Corrupt directory inode reference without permitting an out-of-bounds read. */
    uint32_t root = v7_disk_addr(disk_inode(2)->addr);
    v7_disk_put16(image + root * 512 + 4 * 16, 65); /* hello entry */
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    v7_ino_t ino;
    assert(v7fs_lookup(&fs, 2, "/hello", &ino) == -NX_EIO);
    reset();
    /* Out-of-range indirect pointer is checked before device access. */
    uint32_t indirect = v7_disk_addr(disk_inode(6)->addr + 30);
    v7_disk_put32(image + indirect * 512, 1);
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    assert(v7fs_read(&fs, 6, 10 * 512, buf, 1, &done) == -NX_EIO && done == 0);
    reset();
    /* Invalid chained free list cannot consume the last stack entry. */
    uint32_t chain = v7_disk_u32(super()->free[0]);
    if (!chain) chain = v7_disk_u32(super()->free[1]);
    v7_disk_put16(super()->nfree, 1); v7_disk_put32(super()->free[0], chain);
    v7_disk_put16(image + chain * 512, 51);
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    uint32_t block = UINT32_MAX;
    uint8_t saved_super[512]; memcpy(saved_super, image + 512, 512);
    assert(v7fs_alloc_block(&fs, &block) == -NX_EIO && block == UINT32_MAX);
    assert(!memcmp(saved_super, image + 512, 512));
    reset();
    /* Cached device reads, retry after failed read, and partial-transfer count. */
    struct fault_device fault = {.backing = &ram.device, .bad_read = UINT32_MAX, .bad_write = UINT32_MAX};
    fault.device = (struct block_device){.blocks = ram.device.blocks, .context = &fault,
        .read = fault_read, .write = fault_write};
    assert(v7fs_mount(&fs, &fault.device, false) == 0);
    uint32_t data = v7_disk_addr(disk_inode(6)->addr);
    uint32_t second = v7_disk_addr(disk_inode(6)->addr + 3);
    fault.bad_read = second;
    assert(v7fs_read(&fs, 6, 0, buf, 700, &done) == -NX_EIO && done == 512);
    for (unsigned i = 0; i < done; ++i) assert(buf[i] == (uint8_t)(i * 17 + 3));
    fault.bad_read = UINT32_MAX;
    assert(v7fs_read(&fs, 6, 0, buf, 700, &done) == 0 && done == 700);
    unsigned reads = fault.reads;
    assert(v7fs_read(&fs, 6, 0, buf, 700, &done) == 0 && fault.reads == reads);
    assert(data >= 10);
    fault.bad_write = 1;
    assert(v7fs_write(&fs, 8, 0, "x", 1, &done) == -NX_EIO && done == 0 && fs.failed);
    reads = fault.reads;
    assert(v7fs_read(&fs, 5, 0, buf, 1, &done) == -NX_EIO && fault.reads == reads);
    assert(v7fs_unmount(&fs) == 0);
    fault.bad_write = UINT32_MAX;
    assert(v7fs_mount(&fs, &fault.device, false) == 0);
    assert(v7fs_write(&fs, 8, 0, "x", 1, &done) == 0 && done == 1);
    reset();
    /* ENOSPC after allocating an indirect root keeps it owned and reusable. */
    uint32_t reserve[4];
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    for (unsigned i = 0; i < 4; ++i) assert(v7fs_alloc_block(&fs, &reserve[i]) == 0);
    reset();
    v7_disk_put16(super()->nfree, 2);
    v7_disk_put32(super()->free[0], 0); v7_disk_put32(super()->free[1], reserve[0]);
    v7_disk_put32(super()->tfree, 1);
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    uint64_t triple = (10 + 128 + 128 * 128) * 512UL;
    assert(v7fs_write(&fs, 8, triple, "Z", 1, &done) == -NX_ENOSPC && done == 0 && !fs.failed);
    struct v7fs_inode scratch;
    assert(v7fs_inode_get(&fs, 8, &scratch) == 0 && scratch.size == 0 && scratch.addr[12] == reserve[0]);
    for (unsigned i = 1; i < 4; ++i) assert(v7fs_free_block(&fs, reserve[i]) == 0);
    assert(v7fs_write(&fs, 8, triple, "Z", 1, &done) == 0 && done == 1);
    assert(v7fs_inode_get(&fs, 8, &scratch) == 0 && scratch.addr[12] == reserve[0]);
    assert(v7_disk_u32(fs.super.tfree) == 0);
    assert(v7fs_read(&fs, 8, triple, buf, 1, &done) == 0 && done == 1 && buf[0] == 'Z');
    reset();
    /* An external valid image can contain nonzero bytes beyond file EOF. */
    uint32_t hello = v7_disk_addr(disk_inode(5)->addr);
    uint32_t hello_size = v7_disk_u32(disk_inode(5)->size);
    memset(image + hello * 512 + hello_size, 0xa5, 512 - hello_size);
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    assert(v7fs_write(&fs, 5, 700, "Z", 1, &done) == 0 && done == 1);
    assert(v7fs_read(&fs, 5, hello_size, buf, 700 - hello_size, &done) == 0);
    for (size_t i = 0; i < done; ++i) assert(buf[i] == 0);
    assert(v7fs_read(&fs, 5, 700, buf, 1, &done) == 0 && done == 1 && buf[0] == 'Z');
    assert(v7fs_write(&fs, 5, -1, buf, 1, &done) == -NX_EINVAL);
    assert(v7fs_write(&fs, 5, INT64_MAX, buf, 1, &done) == -NX_EFBIG);
    assert(v7fs_write(&fs, 5, 0, buf, SIZE_MAX, &done) == -NX_EFBIG);
    assert(v7fs_read(&fs, 5, INT64_MAX, buf, SIZE_MAX, &done) == 0 && done == 0);
    reset();
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    /* Reclaim complete single/double/triple trees, including sparse branches. */
    uint32_t available = v7_disk_u32(fs.super.tfree);
    assert(v7fs_truncate(&fs, 6) == 0);
    assert(v7_disk_u32(fs.super.tfree) == available + 154);
    assert(v7fs_truncate(&fs, 7) == 0);
    assert(v7_disk_u32(fs.super.tfree) == available + 158);
    assert(v7fs_inode_get(&fs, 7, &scratch) == 0 && scratch.size == 0);
    for (unsigned i = 0; i < V7_NADDR; ++i) assert(scratch.addr[i] == 0);
    assert(v7fs_truncate(&fs, 2) == -NX_EISDIR);
    reset();
    /* Permission selection uses effective owner/group/other; search checks
     * run at each pathname component, including dot and dot-dot. */
    v7_disk_put16(disk_inode(4)->mode, V7_IFDIR | 0700);
    v7_disk_put16(disk_inode(4)->uid, 123);
    v7_disk_put16(disk_inode(4)->gid, 456);
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    assert(v7fs_lookup_user(&fs, 2, "/tmp/../hello", &ino, 999, 456) == -NX_EACCES);
    assert(v7fs_lookup_user(&fs, 2, "/tmp/../hello", &ino, 123, 999) == 0 && ino == 5);
    assert(v7fs_inode_get(&fs, 4, &scratch) == 0);
    assert(v7fs_access(&scratch, 123, 999, 7));
    assert(!v7fs_access(&scratch, 999, 456, 1));
    scratch.mode = V7_IFDIR | 0750;
    assert(v7fs_access(&scratch, 999, 456, 5) && !v7fs_access(&scratch, 999, 456, 2));
    assert(v7fs_access(&scratch, 0, 999, 7));
    reset();
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    uint16_t free_inodes = v7_disk_u16(fs.super.tinode);
    available = v7_disk_u32(fs.super.tfree);
    char name[V7_DIRSIZ] = "native", alias[V7_DIRSIZ] = "alias";
    assert(v7fs_create(&fs, 4, name, 0640, 123, 456, &ino) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.uid == 123 && scratch.gid == 456);
    assert(v7fs_create(&fs, 4, name, 0640, 123, 456, &ino) == -NX_EEXIST);
    assert(v7fs_write(&fs, ino, 700, "Z", 1, &done) == 0 && done == 1);
    assert(v7fs_link(&fs, ino, 4, alias) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.nlink == 2);
    assert(v7fs_release_inode(&fs, ino) == -NX_EBUSY);
    assert(v7fs_unlink(&fs, 4, name, &ino) == 0);
    assert(v7fs_unlink(&fs, 4, alias, &ino) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.nlink == 0);
    assert(v7fs_read(&fs, ino, 700, buf, 1, &done) == 0 && done == 1 && buf[0] == 'Z');
    assert(v7fs_release_inode(&fs, ino) == 0 && v7fs_inode_get(&fs, ino, &scratch) == -NX_ENOENT);
    assert(v7_disk_u32(fs.super.tfree) == available && v7_disk_u16(fs.super.tinode) == free_inodes);
    /* V7 mkdir/rmdir construct and dismantle directories with privileged
     * mknod/link/unlink; device numbers must never be freed as data blocks. */
    char dirname[V7_DIRSIZ] = "subdir", dot[V7_DIRSIZ] = ".", dotdot[V7_DIRSIZ] = "..";
    v7_ino_t directory, removed;
    assert(v7fs_mknod(&fs, 4, dirname, V7_IFDIR | 0755, 123, 456, 0, &directory) == 0);
    assert(v7fs_link(&fs, directory, directory, dot) == -NX_EPERM);
    assert(v7fs_link_privileged(&fs, directory, directory, dot, true) == 0);
    assert(v7fs_link_privileged(&fs, 4, directory, dotdot, true) == 0);
    assert(v7fs_lookup(&fs, 2, "/tmp/subdir/../subdir/.", &ino) == 0 && ino == directory);
    assert(v7fs_inode_get(&fs, directory, &scratch) == 0 && scratch.nlink == 2 && scratch.size == 32);
    assert(v7fs_truncate(&fs, directory) == -NX_EISDIR);
    assert(v7fs_unlink(&fs, 4, dirname, &removed) == -NX_EPERM);
    assert(v7fs_unlink_privileged(&fs, directory, dotdot, &removed, true) == 0 && removed == 4);
    assert(v7fs_unlink_privileged(&fs, directory, dot, &removed, true) == 0 && removed == directory);
    assert(v7fs_unlink_privileged(&fs, 4, dirname, &removed, true) == 0 && removed == directory);
    assert(v7fs_release_inode(&fs, directory) == 0);
    assert(v7_disk_u32(fs.super.tfree) == available && v7_disk_u16(fs.super.tinode) == free_inodes);
    assert(v7fs_mknod(&fs, 4, dirname, V7_IFCHR | 0600, 0, 0, 0x300, &ino) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.addr[0] == 0x300 && scratch.size == 0);
    assert(v7fs_unmount(&fs) == 0 && v7fs_mount(&fs, &ram.device, false) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.addr[0] == 0x300);
    assert(v7fs_unlink(&fs, 4, dirname, &removed) == 0 && removed == ino);
    assert(v7fs_release_inode(&fs, ino) == 0);
    assert(v7_disk_u32(fs.super.tfree) == available && v7_disk_u16(fs.super.tinode) == free_inodes);
    assert(v7fs_mknod(&fs, 4, dirname, 0, 0, 0, 0, &ino) == -NX_EINVAL);
    assert(v7fs_mknod(&fs, 4, dirname, V7_IFDIR, 0, 0, 123, &ino) == -NX_EINVAL);
    /* Exhaust/reuse the i-list and grow a directory across a sector boundary. */
    v7_ino_t numbers[64];
    for (unsigned i = 0; i < free_inodes; ++i) {
        memset(name, 0, sizeof(name)); snprintf(name, sizeof(name), "file%u", i);
        assert(v7fs_create(&fs, 4, name, 0600, 0, 0, &numbers[i]) == 0);
    }
    memset(name, 0, sizeof(name)); memcpy(name, "overflow", 8);
    ino = 0xffff;
    assert(v7fs_create(&fs, 4, name, 0600, 0, 0, &ino) == -NX_ENOSPC && ino == 0xffff);
    assert(v7fs_unmount(&fs) == 0 && v7fs_mount(&fs, &ram.device, false) == 0);
    for (unsigned i = 0; i < free_inodes; ++i) {
        memset(name, 0, sizeof(name)); snprintf(name, sizeof(name), "file%u", i);
        assert(v7fs_unlink(&fs, 4, name, &ino) == 0 && ino == numbers[i]);
        assert(v7fs_release_inode(&fs, ino) == 0);
    }
    assert(v7_disk_u16(fs.super.tinode) == free_inodes);
    assert(v7_disk_u32(fs.super.tfree) == available - 1); /* Retained directory extension. */
    reset();
    assert(v7fs_mount(&fs, &ram.device, true) == 0);
    assert(v7fs_create(&fs, 4, name, 0600, 0, 0, &ino) == -NX_EROFS);
    assert(v7fs_link(&fs, 5, 4, name) == -NX_EROFS);
    assert(v7fs_unlink(&fs, 4, name, &ino) == -NX_EROFS);
    assert(v7fs_truncate(&fs, 5) == -NX_EROFS);
    assert(v7fs_unmount(&fs) == 0);
    /* Deterministic clock: verify each V7 timestamp independently. */
    reset(); now = 1709210096;
    assert(v7fs_mount(&fs, &ram.device, false) == 0);
    char timed[V7_DIRSIZ] = "timed", alias_time[V7_DIRSIZ] = "time-alias";
    assert(v7fs_create(&fs, 4, timed, 0644, 12, 34, &ino) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0);
    assert(scratch.atime == now && scratch.mtime == now && scratch.ctime == now);
    assert(v7_disk_u32(disk_inode(ino)->mtime) == now);
    assert(v7fs_inode_get(&fs, 4, &scratch) == 0 && scratch.mtime == now && scratch.ctime == now);
    uint32_t parent_access = scratch.atime;
    ++now; v7_ino_t resolved;
    assert(v7fs_lookup(&fs, 4, "timed", &resolved) == 0 && resolved == ino);
    assert(v7fs_inode_get(&fs, 4, &scratch) == 0 && scratch.atime == parent_access);
    assert(v7fs_write(&fs, ino, 0, "abc", 3, &done) == 0 && done == 3);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0);
    assert(scratch.atime == now-1 && scratch.mtime == now && scratch.ctime == now);
    ++now;
    assert(v7fs_write(&fs, ino, 0, "z", 1, &done) == 0 && done == 1);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.mtime == now && scratch.ctime == now);
    ++now;
    assert(v7fs_read(&fs, ino, 0, buf, 1, &done) == 0 && done == 1 && buf[0] == 'z');
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0);
    assert(scratch.atime == now && scratch.mtime == now-1 && scratch.ctime == now-1);
    ++now;
    assert(v7fs_read(&fs, ino, 100, buf, 1, &done) == 0 && done == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.atime == now);
    ++now;
    assert(v7fs_read(&fs, ino, 0, NULL, 0, &done) == 0 && done == 0);
    assert(v7fs_write(&fs, ino, 0, NULL, 0, &done) == 0 && done == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.atime == now-1 && scratch.mtime == now-3);
    uint32_t modified = scratch.mtime, accessed = scratch.atime;
    assert(v7fs_link(&fs, ino, 4, alias_time) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.ctime == now && scratch.mtime == modified);
    ++now;
    assert(v7fs_unlink(&fs, 4, alias_time, &resolved) == 0 && resolved == ino);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.ctime == now && scratch.atime == accessed);
    ++now;
    scratch.mode = V7_IFREG | 0600; scratch.uid = 99; scratch.gid = 88;
    scratch.atime = 100; scratch.mtime = 200; scratch.ctime = 300;
    assert(v7fs_metadata(&fs, &scratch) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.atime == 100 && scratch.mtime == 200 && scratch.ctime == now);
    assert(scratch.uid == 99 && scratch.gid == 88 && scratch.size == 3);
    ++now;
    assert(v7fs_truncate(&fs, ino) == 0);
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.size == 0 && scratch.atime == 100 && scratch.mtime == now && scratch.ctime == now);
    assert(v7fs_sync(&fs) == 0 && v7_disk_u32(super()->time) == now);
    assert(v7fs_unmount(&fs) == 0 && v7fs_mount(&fs, &ram.device, true) == 0);
    uint8_t *saved = malloc(length); assert(saved); memcpy(saved, image, length);
    ++now;
    assert(v7fs_read(&fs, ino, 0, buf, 1, &done) == 0 && done == 0);
    assert(v7fs_sync(&fs) == 0 && !memcmp(saved, image, length));
    assert(v7fs_inode_get(&fs, ino, &scratch) == 0 && scratch.atime == 100 && scratch.mtime == now-1 && scratch.ctime == now-1);
    free(saved);
    assert(v7fs_unmount(&fs) == 0);
    /* Unlinked character inodes own no directory entry or data blocks. */
    reset();assert(v7fs_mount(&fs,&ram.device,false)==0);
    unsigned anonymous_free=v7_disk_u16(super()->tinode);
    uint8_t super_before[V7_BSIZE];memcpy(super_before,super(),V7_BSIZE);
    assert(v7fs_anonymous(&fs,V7_IFREG|0600,12,34,0x500,&ino)==-NX_EINVAL);
    assert(!memcmp(super_before,super(),V7_BSIZE));
    for(unsigned repeat=0;repeat<3;++repeat){
        assert(v7fs_anonymous(&fs,V7_IFCHR|0666,12,34,0x500,&ino)==0);
        assert(v7fs_inode_get(&fs,ino,&scratch)==0 && scratch.nlink==0);
        assert(scratch.mode==(V7_IFCHR|0666) && scratch.uid==12 && scratch.gid==34);
        assert(scratch.addr[0]==0x500 && scratch.size==0 && scratch.ctime==now);
        assert(v7_disk_u16(super()->tinode)==anonymous_free-1);
        assert(v7fs_release_inode(&fs,ino)==0 && v7_disk_u16(super()->tinode)==anonymous_free);
    }
    assert(v7fs_unmount(&fs)==0 && v7fs_mount(&fs,&ram.device,true)==0);
    assert(v7fs_anonymous(&fs,V7_IFCHR|0600,0,0,0x500,&ino)==-NX_EROFS);
    assert(v7fs_unmount(&fs)==0);
    /* Timestamp write faults are reported and poison the write-through mount. */
    reset();
    fault.bad_read = UINT32_MAX; fault.bad_write = v7_itod(5);
    assert(v7fs_mount(&fs, &fault.device, false) == 0);
    assert(v7fs_read(&fs, 5, 0, buf, 1, &done) == -NX_EIO && done == 0 && fs.failed);
    assert(v7fs_unmount(&fs) == 0);
    free(image); free(original);
    puts("PASS: block bounds, V7 metadata validation, cache, device faults, sparse gaps, write errors, permissions, namespace, inode/tree reclamation and V7 timestamp semantics/persistence");
    return 0;
}
