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
static int transfer(struct block_device *device,uint32_t block,void *bytes,bool write)
{
    struct disk_view *v=device->context;struct block_device *base=v->binding->device;
    if(block>=device->blocks || !base || v->first>=base->blocks || block>=base->blocks-v->first)return -NX_EIO;
    if(write && (base->read_only || !base->write))return base->read_only?-NX_EROFS:-NX_ENODEV;
    /* Read errors cannot publish partially filled bytes. Retry only I/O faults;
     * permission, offline and malformed requests are terminal. */
    uint8_t scratch[BLOCK_SIZE];int error=-NX_EIO;
    for(unsigned attempt=0;attempt<=v->retries;++attempt){
        error=write?base->write(base,v->first+block,bytes):base->read(base,v->first+block,scratch);
        if(!error){if(!write)for(unsigned i=0;i<BLOCK_SIZE;++i)((uint8_t *)bytes)[i]=scratch[i];return 0;}
        if(error!=-NX_EIO)break;
    }
    return error;
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
    if(unit>=units[kind] || !length)return -NX_ENXIO;
    struct disk_binding *binding=&bindings[kind][unit];struct block_device *base=binding->device;
    if(!base)return -NX_ENODEV;
    if(first>=base->blocks || length>base->blocks-first)return -NX_ENXIO;
    if(binding->refs==UINT32_MAX)return -NX_ENFILE;
    ++binding->refs;*view=(struct disk_view){.binding=binding,.first=first,.retries=kind==NX_DISK_HP?28:10,
        .device={.blocks=length,.read_only=base->read_only,.read=read_one,.write=base->write?write_one:NULL}};
    view->device.context=view;return 0;
}
void disk_release(struct disk_view *view)
{if(view->binding && view->binding->refs)--view->binding->refs;*view=(struct disk_view){0};}
