/* C/A/T's byte stream with a synchronous native capture-file transport.
 * The open-file description owns the exclusive device and pinned output inode. */
#include "output.h"
#include "uaccess.h"
#include <nexix/errno.h>
static struct output_device { bool open,failed; struct inode *capture; uint64_t offset; } cat;
bool output_matches(v7_dev_t dev){return dev==NX_CAT_DEV;}
bool output_image_busy(const struct inode *inode){return inode && cat.capture==inode;}
int output_open(v7_dev_t dev,unsigned access,void **out)
{
    if(!output_matches(dev))return -NX_ENXIO;
    if(cat.open || access!=NX_O_WRONLY)return -NX_ENXIO;
    cat=(struct output_device){.open=true};*out=&cat;return 0;
}
int output_close(void *handle)
{
    struct output_device *device=handle;int error=0;
    if(device->capture)error=files_inode_put(device->capture);
    *device=(struct output_device){0};return error;
}
int output_ioctl(void *handle,struct address_space *vm,uint64_t request,uintptr_t arg)
{
    (void)vm;struct output_device *device=handle;
    if(request!=NX_OUTPUT_ATTACH)return -NX_ENOTTY;
    if(device->capture)return -NX_EBUSY;
    struct inode *inode;int error=files_tape_backing(arg,true,&inode);
    if(error)return error;
    if(inode->i_size || output_image_busy(inode)){(void)files_inode_put(inode);return -NX_EBUSY;}
    device->capture=inode;return 0;
}
int64_t output_io(void *handle,struct address_space *vm,uintptr_t address,size_t count,bool write)
{
    struct output_device *device=handle;
    if(!write)return -NX_EBADF;
    if(!count)return 0;
    if(!device->capture)return -NX_ENXIO;
    if(device->failed)return -NX_EIO;
    size_t done=0;uint8_t buffer[512];
    while(done<count){
        size_t n=count-done;if(n>sizeof(buffer))n=sizeof(buffer);
        int error=copyin(vm,address+done,buffer,n);
        if(!error)error=files_tape_write(device->capture,device->offset,buffer,n);
        if(error){device->failed=true;return done?(int64_t)done:error;}
        device->offset+=n;done+=n;
    }
    return done;
}
