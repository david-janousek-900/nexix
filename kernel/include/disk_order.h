#ifndef NEXIX_DISK_ORDER_H
#define NEXIX_DISK_ORDER_H
#include "block.h"
struct disk_view;
struct disk_binding;
struct disk_request {
    struct disk_request *next;
    struct disk_view *view;
    struct disk_binding *binding;
    void *bytes;
    uint32_t block,cylinder;
    int error;
    bool read,queued,done;
};
struct disk_queue {struct disk_request *head,*tail;bool running;};
/* V7 dsort/dkleave algorithms; caller serializes access and owns requests. */
void disk_sort(struct disk_queue *,struct disk_request *);
struct disk_request *disk_pop(struct disk_queue *);
uint32_t disk_block(unsigned minor,uint32_t block);
unsigned disk_unit(unsigned minor,uint32_t block);
#endif
