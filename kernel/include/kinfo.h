#ifndef NEXIX_KINFO_H
#define NEXIX_KINFO_H
#include "process.h"
#include <nexix/kinfo.h>
/* IRQs disabled; serializers initialize every byte, and never allocate/sleep/I/O. */
int64_t kinfo_request(uint64_t version,uint64_t kind,uintptr_t buffer,size_t bytes);
bool files_info(unsigned,struct nx_kinfo_file *);
bool inodes_info(unsigned,struct nx_kinfo_inode *);
bool text_info(unsigned,struct nx_kinfo_text *);
unsigned text_info_id(const struct text *);
unsigned files_info_inode_id(const struct inode *);
bool mounts_info(unsigned,struct nx_kinfo_mount *);
void block_statistics(struct nx_kinfo_io *);
void cache_statistics(struct nx_kinfo_io *);
void tty_statistics(struct nx_kinfo_io *);
void cpu_statistics(struct nx_kinfo_io *);
#endif
