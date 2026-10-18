#ifndef NEXIX_OUTPUT_H
#define NEXIX_OUTPUT_H
#include "files.h"
#include <nexix/output.h>
bool output_matches(v7_dev_t);
bool output_image_busy(const struct inode *);
int output_open(v7_dev_t,unsigned,void **);
int output_close(void *);
int64_t output_io(void *,struct address_space *,uintptr_t,size_t,bool);
int output_ioctl(void *,struct address_space *,uint64_t,uintptr_t);
#endif
