#ifndef NEXIX_V7_DISK_H
#define NEXIX_V7_DISK_H
#include "v7/types.h"
#define V7_BSIZE 512U
#define V7_DIRSIZ 14U
#define V7_NADDR 13U
#define V7_NINDIR 128U
#define V7_ROOTINO 2U
#define V7_SUPERB 1U
/* Byte arrays deliberately have no native integer fields or padding. */
struct v7_disk_inode {
    uint8_t mode[2], nlink[2], uid[2], gid[2], size[4];
    uint8_t addr[40], atime[4], mtime[4], ctime[4];
};
struct v7_disk_direct { uint8_t ino[2]; char name[V7_DIRSIZ]; };
struct v7_disk_super {
    uint8_t isize[2], fsize[4], nfree[2], free[50][4];
    uint8_t ninode[2], inode[100][2];
    uint8_t flock, ilock, fmod, ronly;
    uint8_t time[4], tfree[4], tinode[2], m[2], n[2];
    char fname[6], fpack[6];
    uint8_t unused[72];
};
_Static_assert(sizeof(struct v7_disk_inode) == 64, "V7 inode size");
_Static_assert(offsetof(struct v7_disk_inode, atime) == 52, "V7 inode time offset");
_Static_assert(sizeof(struct v7_disk_direct) == 16, "V7 directory size");
_Static_assert(sizeof(struct v7_disk_super) == V7_BSIZE, "V7 superblock size");
_Static_assert(offsetof(struct v7_disk_super, time) == 414, "V7 superblock time offset");
/* PDP-11: little-endian shorts; longs store the high word first.
 * Three-byte addresses follow reference/v7/usr/src/libc/gen/l3.c. */
static inline uint16_t v7_disk_u16(const uint8_t *p)
{ return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static inline void v7_disk_put16(uint8_t *p, uint16_t n)
{ p[0] = n; p[1] = n >> 8; }
static inline uint32_t v7_disk_u32(const uint8_t *p)
{ return (uint32_t)v7_disk_u16(p) << 16 | v7_disk_u16(p + 2); }
static inline void v7_disk_put32(uint8_t *p, uint32_t n)
{ v7_disk_put16(p, n >> 16); v7_disk_put16(p + 2, n); }
static inline uint32_t v7_disk_addr(const uint8_t *p)
{ return (uint32_t)p[0] << 16 | (uint32_t)p[2] << 8 | p[1]; }
/* Reject overflow rather than silently changing an inode block number. */
static inline int v7_disk_putaddr(uint8_t *p, uint32_t n)
{
    if (n > 0xffffffU) return 0;
    p[0] = n >> 16; p[1] = n; p[2] = n >> 8;
    return 1;
}
/* Inode zero is invalid. Widen before adding to avoid 16-bit wrap. */
static inline uint32_t v7_itod(v7_ino_t ino) { return ((uint32_t)ino + 15) / 8; }
static inline unsigned v7_itoo(v7_ino_t ino) { return ((uint32_t)ino + 15) % 8; }
#endif
