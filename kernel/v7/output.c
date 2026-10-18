/* Native capture transports for V7 cat, vp and vs output protocols. */
#include "output.h"
#include "uaccess.h"
#include <nexix/errno.h>
struct output_device {
    unsigned opens,kind,mode,pending_go;
    bool failed;
    struct inode *capture;
    uint64_t offset;
    uint8_t input[11];unsigned head,count;
};
static struct output_device devices[3];
bool output_matches(v7_dev_t dev){return dev>=NX_CAT_DEV && dev<=NX_VS_DEV;}
bool output_image_busy(const struct inode *inode)
{
    for(unsigned i=0;i<3;++i)if(inode && devices[i].capture==inode)return true;
    return false;
}
static int emit(struct output_device *device,const void *bytes,size_t size)
{
    if(device->failed)return -NX_EIO;
    int error=files_tape_write(device->capture,device->offset,bytes,size);
    if(error){device->failed=true;return error;}
    device->offset+=size;return 0;
}
static int go(struct output_device *device)
{
    if(!device->capture){++device->pending_go;return 0;}
    const uint8_t byte=026;return emit(device,&byte,1);
}
/* VP records are explicitly little endian, independent of C structure layout. */
static int vp_record(struct output_device *device,unsigned type,unsigned mode,const void *data,size_t size)
{
    uint8_t header[8]={(uint8_t)type,0,(uint8_t)mode,(uint8_t)(mode>>8),
        (uint8_t)size,(uint8_t)(size>>8),0,0};
    int error=emit(device,header,sizeof(header));
    return error || !size ? error : emit(device,data,size);
}
static int vp_commands(struct output_device *device,unsigned commands)
{
    for(unsigned bit=2;bit<=040;bit<<=1)if(commands&bit){
        int error=vp_record(device,NX_VP_COMMAND,bit,0,0);if(error)return error;
    }
    return 0;
}
int output_open(v7_dev_t dev,unsigned access,void **out)
{
    if(!output_matches(dev))return -NX_ENXIO;
    unsigned kind=dev-NX_CAT_DEV;struct output_device *device=&devices[kind];
    if((kind!=2 && (device->opens || access!=NX_O_WRONLY)))return -NX_ENXIO;
    if(!device->opens)*device=(struct output_device){.kind=kind,.mode=NX_VPRINT};
    if(kind==2){int error=go(device);if(error)return error;}
    ++device->opens;*out=device;return 0;
}
int output_close(void *handle)
{
    struct output_device *device=handle;int error=0;
    if(device->kind==2){error=go(device);device->count=device->head=0;proc_wakeup(device);}
    if(--device->opens)return error;
    if(device->capture){int e=files_inode_put(device->capture);if(!error)error=e;}
    *device=(struct output_device){0};return error;
}
int output_ioctl(void *handle,struct address_space *vm,uint64_t request,uintptr_t arg)
{
    struct output_device *device=handle;
    if(device->kind==1 && request==NX_VGETSTATE){
        uint16_t state=1|device->mode;return copyout(vm,arg,&state,sizeof(state));
    }
    if(device->kind==1 && request==NX_VSETSTATE){
        uint16_t state;int error=copyin(vm,arg,&state,sizeof(state));if(error)return error;
        if(!device->capture)return -NX_ENXIO;
        if(device->failed)return -NX_EIO;
        error=vp_commands(device,state&076);if(error)return error;
        device->mode=state&0700;return 0;
    }
    if(device->kind==2 && request==NX_VS_INPUT){
        struct nx_vs_input input;int error=copyin(vm,arg,&input,sizeof(input));if(error)return error;
        if(input.reserved || input.count>11)return -NX_EINVAL;
        if(input.count>11-device->count)return -NX_ENOSPC;
        for(unsigned i=0;i<input.count;++i)device->input[(device->head+device->count+i)%11]=input.codes[i];
        device->count+=input.count;proc_wakeup(device);return 0;
    }
    if(request!=NX_OUTPUT_ATTACH)return -NX_ENOTTY;
    if(device->capture)return -NX_EBUSY;
    struct inode *inode;int error=files_tape_backing(arg,true,&inode);
    if(error)return error;
    if(inode->i_size || output_image_busy(inode)){(void)files_inode_put(inode);return -NX_EBUSY;}
    device->capture=inode;
    if(device->kind==1){
        static const uint8_t magic[8]={'N','X','V','P',1,0,0,0};
        error=emit(device,magic,sizeof(magic));
        if(!error)error=vp_commands(device,026); /* RESET, CLRCOM, FFCOM */
    }else if(device->kind==2){
        unsigned pending=device->pending_go;device->pending_go=0;
        while(pending-- && !error)error=go(device);
    }
    return error;
}
int64_t output_io(void *handle,struct address_space *vm,uintptr_t address,size_t count,bool write)
{
    struct output_device *device=handle;
    if(!count)return 0;
    if(!write){
        if(device->kind!=2)return -NX_EBADF;
        while(!device->count)if(proc_sleep_interruptible(device,20))return -NX_EINTR;
        uint8_t byte="?0*#?546?213?879?"[device->input[device->head]&017];
        int error=copyout(vm,address,&byte,1);if(error)return error;
        device->head=(device->head+1)%11;--device->count;return 1;
    }
    if(!device->capture)return -NX_ENXIO;
    if(device->failed)return -NX_EIO;
    size_t done=0;uint8_t buffer[512];
    while(done<count){
        size_t n=count-done;if(n>sizeof(buffer))n=sizeof(buffer);
        int error=copyin(vm,address+done,buffer,n);
        if(!error && device->kind==0)error=emit(device,buffer,n);
        else if(!error && device->kind==1){
            error=vp_record(device,NX_VP_DATA,device->mode,buffer,n);
            if(!error && device->mode==NX_VPPLOT)device->mode=NX_VPLOT;
        }else if(!error){
            /* Reference vswrite resets its 120-byte burst counter per write. */
            for(size_t i=0;i<n && !error;++i){
                if((done+i)%120==0)error=go(device);
                if(!error)error=emit(device,&buffer[i],1);
            }
        }
        if(error){device->failed=true;return done?(int64_t)done:error;}
        done+=n;
    }
    if(device->kind==2){int error=go(device);if(error)return error;}
    return done;
}
