#ifndef NEXIX_NAMESPACE_H
#define NEXIX_NAMESPACE_H
#include "v7/fs.h"
#define NAMESPACE_MOUNTS 8
struct inode;
struct fs_node { v7_dev_t dev; v7_ino_t ino; };
static inline bool node_equal(struct fs_node a,struct fs_node b)
{ return a.dev==b.dev && a.ino==b.ino; }
/* IRQs disabled; nodes contain stable device/inode identities, not user pointers. */
struct v7fs *namespace_volume(v7_dev_t);
int namespace_root(struct v7fs *); /* Quiescent root installation. */
int namespace_lookup(struct fs_node cwd,struct fs_node root,const char *,uint16_t uid,uint16_t gid,struct fs_node *);
int namespace_mount(v7_dev_t,struct fs_node,struct inode *pin,bool read_only);
int namespace_mount_file(v7_dev_t,struct fs_node,struct fs_node parent,struct inode *pin,bool read_only);
bool namespace_mountpoint(struct fs_node);
unsigned namespace_pins(void);
bool namespace_quiescent(void);
int namespace_unmount(v7_dev_t);
int namespace_reset(void); /* Quiescent kernel teardown of all secondary volumes. */
int namespace_sync(void);
#endif
