#include "storage.h"
#include <nexix/errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct block_device media[2];
static unsigned unitlog[128],blocklog[128],used,failures;static bool reenter;
static struct disk_view *active;
static int read_one(struct block_device *d,uint32_t b,void *bytes)
{
    assert(used<128);unitlog[used]=d-media;blocklog[used++]=b;memset(bytes,(b+(d-media)*31)&255,512);
    if(reenter){reenter=false;assert(active->device.read(&active->device,0,bytes)==-NX_EBUSY);}
    if(failures){--failures;return -NX_EIO;}return 0;
}
static int write_one(struct block_device *d,uint32_t b,const void *bytes)
{(void)bytes;assert(used<128);unitlog[used]=d-media;blocklog[used++]=b;return 0;}
int main(void)
{
    struct disk_queue q={0};struct disk_request r[8]={0};unsigned cylinders[]={50,10,30,20,20,5,25,100};
    for(unsigned i=0;i<8;++i){r[i].cylinder=cylinders[i];r[i].read=i<3 || i==4 || i==7;disk_sort(&q,&r[i]);assert(q.head==&r[0]);}
    unsigned expected[]={0,7,1,4,2,3,6,5};for(unsigned i=0;i<8;++i)assert(disk_pop(&q)==&r[expected[i]]);
    assert(!q.head && !q.tail && !disk_pop(&q));
    for(unsigned u=0;u<2;++u){media[u]=(struct block_device){.blocks=340670,.read=read_one,.write=write_one};assert(!disk_bind(NX_DISK_HP,u,&media[u]));}
    unsigned minor=NX_HP_INTERLEAVED_MINOR(2,0);struct disk_view v;
    assert(!disk_acquire(NX_DISK_DEV(NX_DISK_HP,minor,false),false,&v) && v.device.blocks==19228);
    uint8_t bytes[8][512];struct disk_request batch[8]={0};active=&v;
    /* Both physical queues use per-drive cylinder order, retaining their heads. */
    unsigned blocks[]={1672,0,836,1673,1,837};
    for(unsigned i=0;i<6;++i)assert(!disk_submit(&v,&batch[i],blocks[i],bytes[i],false));
    assert(disk_release(&v)==-NX_EBUSY && disk_bind(NX_DISK_HP,1,NULL)==-NX_EBUSY);
    assert(disk_submit(&v,&batch[0],0,bytes[0],false)==-NX_EBUSY);
    assert(disk_submit(&v,&r[0],19228,bytes[0],false)==-NX_EIO);
    used=0;reenter=true;assert(!disk_dispatch(v.binding) && !disk_dispatch(v.other));
    unsigned order[]={836,0,418,836,0,418};
    for(unsigned i=0;i<6;++i){assert(batch[i].done && !batch[i].queued && !batch[i].error);assert(blocklog[i]==order[i] && unitlog[i]==i/3);}
    assert(!v.pending && !disk_release(&v));
    assert(!disk_acquire(NX_DISK_DEV(NX_DISK_HP,4,false),false,&v));used=0;active=&v;
    memset(batch,0,sizeof(batch));
    for(unsigned i=0;i<8;++i)assert(!disk_submit(&v,&batch[i],cylinders[i]*418,bytes[i],!r[i].read));
    assert(!disk_dispatch(v.binding));
    for(unsigned i=0;i<8;++i)assert(blocklog[i]==18392+cylinders[expected[i]]*418 && batch[i].done && !batch[i].error);
    assert(!disk_release(&v));
    /* Different partitions share the physical unit queue and cylinder space. */
    struct disk_view partition;
    assert(!disk_acquire(NX_DISK_DEV(NX_DISK_HP,0,false),false,&v));
    assert(!disk_acquire(NX_DISK_DEV(NX_DISK_HP,4,false),false,&partition));
    used=0;memset(batch,0,sizeof(batch));
    assert(!disk_submit(&v,&batch[0],836,bytes[0],false));
    assert(!disk_submit(&partition,&batch[1],0,bytes[1],false));
    assert(!disk_submit(&v,&batch[2],1254,bytes[2],false));
    assert(batch[0].cylinder==2 && batch[1].cylinder==44 && batch[2].cylinder==3);
    assert(!disk_dispatch(v.binding) && blocklog[0]==836 && blocklog[1]==1254 && blocklog[2]==18392);
    assert(!disk_release(&partition) && !disk_release(&v));
    /* Pin validation must roll back cleanly when any member is absent/short. */
    assert(!disk_bind(NX_DISK_HP,1,NULL));
    assert(disk_acquire(NX_DISK_DEV(NX_DISK_HP,minor,false),false,&v)==-NX_ENODEV);
    assert(!disk_bind(NX_DISK_HP,0,&media[0]));media[1].blocks=1;assert(!disk_bind(NX_DISK_HP,1,&media[1]));
    assert(disk_acquire(NX_DISK_DEV(NX_DISK_HP,minor,false),false,&v)==-NX_ENXIO);media[1].blocks=340670;
    assert(!disk_bind(NX_DISK_HP,1,&media[0]));assert(disk_acquire(NX_DISK_DEV(NX_DISK_HP,minor,false),false,&v)==-NX_ENXIO);
    assert(!disk_bind(NX_DISK_HP,1,&media[1]));
    assert(disk_acquire(NX_DISK_DEV(NX_DISK_HP,minor,true),true,&v)==-NX_ENXIO);
    assert(disk_acquire(NX_DISK_DEV(NX_DISK_HP,NX_HP_INTERLEAVED_MINOR(3,0),false),false,&v)==-NX_ENXIO);
    struct storage_handle *h,*mounted,*other;size_t n;
    assert(!storage_open(NX_DISK_DEV(NX_DISK_HP,minor,false),false,true,&h));
    used=0;assert(!storage_transfer(h,0,bytes,1024,false,&n) && n==1024 && unitlog[0]==0 && unitlog[1]==1 && !blocklog[0] && !blocklog[1]);
    used=0;assert(!storage_transfer(h,19227ULL*512,bytes,512,false,&n) && unitlog[0]==1 && blocklog[0]==9613);
    media[1].read_only=true;assert(storage_transfer(h,0,bytes,1,true,&n)==-NX_EROFS && !n);media[1].read_only=false;
    assert(!storage_mount_open(NX_DISK_DEV(NX_DISK_HP,8,false),false,&mounted));
    assert(storage_transfer(h,0,bytes,1,true,&n)==-NX_EBUSY);
    assert(storage_open(NX_DISK_DEV(NX_DISK_HP,minor,true),true,true,&other)==-NX_ENXIO);
    storage_mount_close(mounted);storage_close(h);
    assert(!storage_mount_open(NX_DISK_DEV(NX_DISK_HP,minor,false),false,&mounted));
    for(unsigned u=0;u<2;++u)assert(storage_open(NX_DISK_DEV(NX_DISK_HP,u*8,true),true,true,&h)==-NX_EBUSY);
    assert(!storage_open(NX_DISK_DEV(NX_DISK_HP,1,false),false,true,&other));storage_close(other); /* adjacent partition */
    storage_mount_close(mounted);
    /* An error completes its request and lets the next queued request run. */
    assert(!disk_acquire(NX_DISK_DEV(NX_DISK_HP,0,false),false,&v));active=&v;used=0;failures=29;
    memset(batch,0,sizeof(batch));memset(bytes,0xa5,sizeof(bytes));
    assert(!disk_submit(&v,&batch[0],0,bytes[0],false) && !disk_submit(&v,&batch[1],1,bytes[1],false));
    assert(!disk_dispatch(v.binding) && batch[0].error==-NX_EIO && !batch[1].error && batch[0].done && batch[1].done && used==30);
    for(unsigned i=0;i<512;++i)assert(bytes[0][i]==0xa5);
    assert(!disk_release(&v));for(unsigned u=0;u<2;++u)assert(!disk_bind(NX_DISK_HP,u,NULL));
    puts("PASS: V7 queue insertion, physical dispatch, completion/retries, reentry, interleave bounds/pins, both-member mount guards and live write protection");
}
