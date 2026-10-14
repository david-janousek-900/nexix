#ifndef NEXIX_TEXT_H
#define NEXIX_TEXT_H
#include "v7/core.h"
#define V7_NTEXT 40
/* IRQs disabled. One held inode per text entry; resident private VM copies. */
int text_acquire(struct inode *,size_t,struct text **);
bool text_valid(const struct text *);
void text_retain(struct text *);
void text_release(struct text *);
int text_evict(struct inode *); /* Only unused entries; active ones remain. */
int text_purge_unused(void);
int text_purge_device(v7_dev_t); /* V7 xumount: unused text on this device only. */
int files_inode_put(struct inode *); /* Managed inode ownership callback. */
#endif
