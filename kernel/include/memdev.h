#ifndef NEXIX_MEMDEV_H
#define NEXIX_MEMDEV_H
#include "process.h"
#include <nexix/memdev.h>
#include <nexix/file.h>
bool memdev_matches(v7_dev_t);
int memdev_open(v7_dev_t,struct user *);
/* Serialized, never sleeps/allocates; validate entire range before transfer.
 * Offsets use physical bytes (mem), kernel virtual bytes (kmem). */
int64_t memdev_io(v7_dev_t,struct user *,struct address_space *,int64_t,uintptr_t,size_t,bool);
#endif
