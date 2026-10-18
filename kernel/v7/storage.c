/* Native device switches and synchronous physio bounce-buffer equivalent. */
#include "storage.h"
#include "dectape.h"
#include <nexix/file.h>
#include <nexix/errno.h>
static struct storage_handle devices[STORAGE_SLOTS],views[32];
bool storage_block_device(uint16_t d){return ((d>>8)==NX_BLOCK_DISK_MAJOR && (d&255)<STORAGE_SLOTS) || disk_major(d>>8,false) || tc_major(d>>8);}
bool storage_raw_device(uint16_t d){return (d>>8)==NX_RAW_DISK_MAJOR || disk_major(d>>8,true);}
static struct block_device *backing(struct storage_handle *h)
{return h->dectape?tc_backing(h->device):h->controller?h->view.binding->device:h->device;}
static bool overlaps(struct storage_handle *a,struct storage_handle *b)
{
    if(!a->device || !b->device || backing(a)!=backing(b))return false;
    uint64_t x=a->controller?a->view.first:0,y=b->controller?b->view.first:0;
    return x<y+b->device->blocks && y<x+a->device->blocks;
}
static bool mounted_overlap(struct storage_handle *h)
{
    for(unsigned i=0;i<STORAGE_SLOTS;++i)if(devices[i].mounted && overlaps(h,&devices[i]))return true;
    for(unsigned i=0;i<32;++i)if(views[i].refs && views[i].mounted && overlaps(h,&views[i]))return true;
    return false;
}
int storage_bind(unsigned minor, struct block_device *device, bool mounted)
{
    if(minor>=STORAGE_SLOTS)return -NX_ENODEV;
    if(devices[minor].refs || disk_backing_bound(devices[minor].device) || tc_bound(devices[minor].device))return -NX_EBUSY;
    if(device&&(!device->blocks||!device->read))return -NX_ENODEV;
    devices[minor]=(struct storage_handle){.device=device,.mounted=mounted};return 0;
}
int storage_open(uint16_t dev,bool raw,bool write,struct storage_handle **result)
{
    unsigned major=dev>>8,minor=dev&255;
    struct storage_handle *h=NULL;
    if(major==(raw?NX_RAW_DISK_MAJOR:NX_BLOCK_DISK_MAJOR)){
        if(minor>=STORAGE_SLOTS || !devices[minor].device)return -NX_ENODEV;
        h=&devices[minor];
    }else if(!raw && tc_major(major)){
        for(unsigned i=0;i<32;++i)if(views[i].refs && views[i].dev==dev){h=&views[i];break;}
        if(!h){
            for(unsigned i=0;i<32;++i)if(!views[i].refs){h=&views[i];break;}
            if(!h)return -NX_ENFILE;
            int error=tc_acquire(dev,&h->device);if(error)return error;
            h->dectape=true;h->dev=dev;
        }
    }else if(disk_major(major,raw)){
        for(unsigned i=0;i<32;++i)if(views[i].refs && views[i].dev==dev){h=&views[i];break;}
        if(!h){
            for(unsigned i=0;i<32;++i)if(!views[i].refs){h=&views[i];break;}
            if(!h)return -NX_ENFILE;
            int error=disk_acquire(dev,raw,&h->view);if(error)return error;
            h->device=&h->view.device;h->controller=true;h->dev=dev;
        }
    }else return -NX_ENODEV;
    if(h->controller || h->dectape)h->device->read_only=backing(h)->read_only;
    int error=0;
    if(write && h->device->read_only)error=-NX_EROFS;
    else if(write && mounted_overlap(h))error=-NX_EBUSY;
    else if(write && !h->device->write)error=-NX_ENODEV;
    if(error){if((h->controller || h->dectape) && !h->refs){if(h->dectape)tc_release(h->device);else disk_release(&h->view);*h=(struct storage_handle){0};}return error;}
    ++h->refs;*result=h;return 0;
}
int storage_mount_open(uint16_t dev,bool read_only,struct storage_handle **result)
{
    int error=storage_open(dev,false,!read_only,result);if(error)return error;
    if(mounted_overlap(*result)){storage_close(*result);return -NX_EBUSY;}
    (*result)->mounted=true;return 0;
}
void storage_mount_close(struct storage_handle *h){h->mounted=false;storage_close(h);}
void storage_close(struct storage_handle *h)
{if(h && h->refs && !--h->refs && (h->controller || h->dectape)){if(h->dectape)tc_release(h->device);else disk_release(&h->view);*h=(struct storage_handle){0};}}
int storage_transfer(struct storage_handle *h,uint64_t offset,void *buffer,size_t count,bool write,size_t *done)
{
    *done=0;
    if(!h||!h->refs||!h->device)return -NX_ENODEV;
    if(h->controller || h->dectape)h->device->read_only=backing(h)->read_only;
    if(write&&h->device->read_only)return -NX_EROFS;
    if(write&&mounted_overlap(h))return -NX_EBUSY;
    uint64_t limit=(uint64_t)h->device->blocks*BLOCK_SIZE;
    if(offset>limit)return -NX_EIO;
    if(count&&!buffer)return -NX_EINVAL;
    if(!count)return 0;
    if(offset==limit)return write||h->dectape?-NX_EIO:0;
    uint8_t sector[BLOCK_SIZE],*bytes=buffer;
    while(*done<count&&offset<limit){
        unsigned within=offset% BLOCK_SIZE;size_t n=BLOCK_SIZE-within;
        if(n>count-*done)n=count-*done;
        int error=0;uint32_t block=offset/BLOCK_SIZE;
        if(!write||within||n!=BLOCK_SIZE)error=block_read(h->device,block,1,sector);
        if(error)return error;
        if(write){
            for(size_t i=0;i<n;++i)sector[within+i]=bytes[*done+i];
            error=block_write(h->device,block,1,sector);
            if(error)return error;
        }else for(size_t i=0;i<n;++i)bytes[*done+i]=sector[within+i];
        *done+=n;offset+=n;
    }
    return write&&*done<count?-NX_EIO:0;
}
