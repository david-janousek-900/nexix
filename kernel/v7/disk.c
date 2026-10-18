/* V7 hp/rf/rk/rl/rp: native LBA views, partition/unit checks and recovery. */
#include "disk.h"
#include <nexix/errno.h>
static int read_one(struct block_device *,uint32_t,void *);
static struct disk_binding bindings[NX_DISK_KINDS][8];
static const unsigned units[]={2,1,4,4,8};
static const uint32_t hp_length[]={9614,8778,0,0,161348,160930,153406,322278};
static const uint32_t hp_first[]={0,9614,0,0,18392,179740,18392,18392};
static const uint32_t rp_length[]={81000,5000,2000,74000,0,0,0,0};
static const uint32_t rp_first[]={0,0,5000,7000,0,0,0,0};
bool disk_major(unsigned major,bool raw)
{return major>=(raw?12U:7U) && major<(raw?17U:12U);}
bool disk_backing_bound(struct block_device *device)
{
    if(!device)return false;
    for(unsigned k=0;k<NX_DISK_KINDS;++k)for(unsigned u=0;u<units[k];++u)
        if(bindings[k][u].device==device)return true;
    return false;
}
int disk_bind(unsigned kind,unsigned unit,struct block_device *device)
{
    if(kind>=NX_DISK_KINDS || unit>=units[kind])return -NX_ENODEV;
    struct disk_binding *b=&bindings[kind][unit];if(b->refs)return -NX_EBUSY;
    if(device && (!device->blocks || !device->read || device->read==read_one))return -NX_ENODEV;
    *b=(struct disk_binding){.device=device};return 0;
}
static struct disk_binding *member(const struct disk_view *v,unsigned i)
{return i?v->other:v->binding;}
bool disk_read_only(const struct disk_view *v)
{for(unsigned i=0;i<v->members;++i)if(member(v,i)->device->read_only)return true;return false;}
bool disk_region(const struct disk_view *v,unsigned i,struct block_device **base,uint32_t *first,uint32_t *length)
{
    if(i>=v->members)return false;
    *base=member(v,i)->device;*first=v->first;*length=v->device.blocks/v->members;return true;
}
static struct disk_binding *target(struct disk_view *v,uint32_t block)
{return member(v,v->members>1?disk_unit(v->minor,block):0);}
static int perform(struct disk_view *v,uint32_t block,void *bytes,bool write)
{
    struct block_device *base=target(v,block)->device;
    uint32_t physical=v->kind==NX_DISK_HP?disk_block(v->minor,block):block;
    if(block>=v->device.blocks || !base || v->first>=base->blocks || physical>=base->blocks-v->first)return -NX_EIO;
    if(write && disk_read_only(v))return -NX_EROFS;
    if(write && !base->write)return -NX_ENODEV;
    uint8_t scratch[BLOCK_SIZE];int error=-NX_EIO;
    for(unsigned attempt=0;attempt<=v->retries;++attempt){
        error=write?base->write(base,v->first+physical,bytes):base->read(base,v->first+physical,scratch);
        if(!error){if(!write)for(unsigned i=0;i<BLOCK_SIZE;++i)((uint8_t *)bytes)[i]=scratch[i];return 0;}
        if(error!=-NX_EIO)break;
    }
    return error;
}
int disk_submit(struct disk_view *v,struct disk_request *r,uint32_t block,void *bytes,bool write)
{
    if(!v || !v->binding || v->kind!=NX_DISK_HP || !r || !bytes)return -NX_EINVAL;
    if(r->queued)return -NX_EBUSY;
    if(block>=v->device.blocks)return -NX_EIO;
    if(v->pending==UINT32_MAX)return -NX_ENFILE;
    struct disk_binding *b=target(v,block);
    *r=(struct disk_request){.view=v,.binding=b,.block=block,.bytes=bytes,.read=!write,.queued=true,
        .cylinder=(v->first+disk_block(v->minor,block))/418};
    ++v->pending;disk_sort(&b->queue,r);return 0;
}
int disk_dispatch(struct disk_binding *binding)
{
    if(!binding || !binding->device)return -NX_ENODEV;
    struct disk_queue *q=&binding->queue;if(q->running)return -NX_EBUSY;
    q->running=true;
    while(q->head){
        struct disk_request *r=q->head;
        r->error=perform(r->view,r->block,r->bytes,!r->read);
        (void)disk_pop(q);
        --r->view->pending;r->queued=false;r->done=true;
    }
    q->running=false;return 0;
}
static int transfer(struct block_device *device,uint32_t block,void *bytes,bool write)
{
    struct disk_view *v=device->context;
    if(block>=device->blocks)return -NX_EIO;
    if(v->kind!=NX_DISK_HP)return perform(v,block,bytes,write);
    struct disk_binding *b=target(v,block);
    /* A synchronous recursive callback cannot leave a stack request queued. */
    if(b->queue.running)return -NX_EBUSY;
    struct disk_request request={0};int error=disk_submit(v,&request,block,bytes,write);
    if(error)return error;
    error=disk_dispatch(b);return error?error:request.error;
}
static int read_one(struct block_device *d,uint32_t b,void *p){return transfer(d,b,p,false);}
static int write_one(struct block_device *d,uint32_t b,const void *p){return transfer(d,b,(void *)p,true);}
int disk_acquire(uint16_t dev,bool raw,struct disk_view *view)
{
    unsigned major=dev>>8,minor=dev&255;if(!disk_major(major,raw))return -NX_ENODEV;
    unsigned kind=major-(raw?12:7),unit=minor,part=minor&7;uint32_t first=0,length=0;
    if(kind==NX_DISK_HP || kind==NX_DISK_RP){
        unit=minor>>3;first=kind==NX_DISK_HP?hp_first[part]:rp_first[part];
        length=kind==NX_DISK_HP?hp_length[part]:rp_length[part];
    }else if(kind==NX_DISK_RF){if(minor>=8)return -NX_ENXIO;unit=0;length=1024U*(minor+1);}
    else length=kind==NX_DISK_RK?4872:10240;
    unsigned count=1;
    if(kind==NX_DISK_HP){
        if(minor&0200 || (raw && (minor&NX_HP_INTERLEAVE)))return -NX_ENXIO;
        if(minor&NX_HP_INTERLEAVE){count=((minor>>3)&7)+1;unit=0;}
    }
    if(unit>=units[kind] || count>units[kind] || !length)return -NX_ENXIO;
    struct disk_binding *selected[2]={&bindings[kind][unit],NULL};
    if(count>1)selected[1]=&bindings[kind][1];
    bool readonly=false,writable=true;
    for(unsigned i=0;i<count;++i){
        struct disk_binding *b=selected[i];struct block_device *base=b->device;
        if(!base)return -NX_ENODEV;
        if(first>=base->blocks || length>base->blocks-first)return -NX_ENXIO;
        if(i && base==selected[0]->device)return -NX_ENXIO;
        if(b->refs==UINT32_MAX)return -NX_ENFILE;
        readonly|=base->read_only;writable&=base->write!=NULL;
    }
    for(unsigned i=0;i<count;++i)++selected[i]->refs;
    *view=(struct disk_view){.binding=selected[0],.other=selected[1],.members=count,.kind=kind,.minor=minor,
        .first=first,.retries=kind==NX_DISK_HP?28:10,
        .device={.blocks=length*count,.read_only=readonly,.read=read_one,.write=writable?write_one:NULL}};
    view->device.context=view;return 0;
}
int disk_release(struct disk_view *view)
{
    if(view->pending)return -NX_EBUSY;
    for(unsigned i=0;i<view->members;++i)if(member(view,i)->refs)--member(view,i)->refs;
    *view=(struct disk_view){0};return 0;
}
