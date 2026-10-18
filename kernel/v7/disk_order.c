/* Adapted from V7 dev/{dsort,dkleave}.c; preserve the original insertion order. */
#include "disk_order.h"
void disk_sort(struct disk_queue *q,struct disk_request *r)
{
    struct disk_request *a=q->head,*at=NULL;
    if(!a){q->head=q->tail=r;r->next=NULL;return;}
    for(;a;a=a->next){
        if(r->read && !a->read){if(!at)at=a;break;}
        if(!r->read && a->read)continue;
        if(a->cylinder<=r->cylinder && (!at || a->cylinder>=at->cylinder))at=a;
    }
    if(!at)at=q->tail;
    r->next=at->next;at->next=r;if(at==q->tail)q->tail=r;
}
struct disk_request *disk_pop(struct disk_queue *q)
{
    struct disk_request *r=q->head;if(!r)return NULL;
    q->head=r->next;if(!q->head)q->tail=NULL;r->next=NULL;return r;
}
uint32_t disk_block(unsigned minor,uint32_t block)
{return minor&0100?block/(((minor>>3)&7)+1):block;}
unsigned disk_unit(unsigned minor,uint32_t block)
{return minor&0100?block%(((minor>>3)&7)+1):minor>>3;}
