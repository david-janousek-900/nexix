/* Native device switches and synchronous physio bounce-buffer equivalent. */
#include "storage.h"
#include <nexix/file.h>
#include <nexix/errno.h>
static struct storage_handle devices[STORAGE_SLOTS];
int storage_bind(unsigned minor, struct block_device *device, bool mounted)
{
    if(minor>=STORAGE_SLOTS)return -NX_ENODEV;
    if(devices[minor].refs)return -NX_EBUSY;
    if(device&&(!device->blocks||!device->read))return -NX_ENODEV;
    devices[minor]=(struct storage_handle){.device=device,.mounted=mounted};return 0;
}
int storage_open(uint16_t dev,bool raw,bool write,struct storage_handle **result)
{
    unsigned major=dev>>8,minor=dev&255;
    if(major!=(raw?NX_RAW_DISK_MAJOR:NX_BLOCK_DISK_MAJOR)||minor>=STORAGE_SLOTS||!devices[minor].device)return -NX_ENODEV;
    struct storage_handle *h=&devices[minor];
    if(write&&h->device->read_only)return -NX_EROFS;
    if(write&&h->mounted)return -NX_EBUSY;
    if(write&&!h->device->write)return -NX_ENODEV;
    ++h->refs;*result=h;return 0;
}
void storage_close(struct storage_handle *h){if(h&&h->refs)--h->refs;}
int storage_transfer(struct storage_handle *h,uint64_t offset,void *buffer,size_t count,bool write,size_t *done)
{
    *done=0;
    if(!h||!h->refs||!h->device)return -NX_ENODEV;
    if(write&&h->device->read_only)return -NX_EROFS;
    if(write&&h->mounted)return -NX_EBUSY;
    uint64_t limit=(uint64_t)h->device->blocks*BLOCK_SIZE;
    if(offset>limit)return -NX_EIO;
    if(count&&!buffer)return -NX_EINVAL;
    if(!count)return 0;
    if(offset==limit)return write?-NX_EIO:0;
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
