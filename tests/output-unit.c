#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "output.h"
#include "uaccess.h"
#include <nexix/errno.h>
static struct inode inode;
static int pins,writes,fail_at;
static unsigned char capture[4096];
int files_tape_backing(uintptr_t address,bool write,struct inode **out)
{(void)address;assert(write);++pins;*out=&inode;return 0;}
int files_inode_put(struct inode *p){assert(p==&inode && pins);--pins;return 0;}
int files_tape_write(struct inode *p,uint64_t off,const void *data,size_t size)
{
    assert(p==&inode);if(++writes==fail_at)return -NX_ENOSPC;
    assert(off+size<=sizeof(capture));memcpy(capture+off,data,size);return 0;
}
int copyin(const struct address_space *vm,uintptr_t address,void *data,size_t size)
{(void)vm;if(address<4096)return -NX_EFAULT;memcpy(data,(void *)address,size);return 0;}
int copyout(const struct address_space *vm,uintptr_t address,const void *data,size_t size)
{(void)vm;if(address<4096)return -NX_EFAULT;memcpy((void *)address,data,size);return 0;}
unsigned proc_wakeup(void *channel){(void)channel;return 0;}
int proc_sleep_interruptible(void *channel,unsigned priority){(void)channel;(void)priority;return 1;}
int main(void)
{
    void *handle,*other;unsigned char bytes[1025];memset(bytes,0xab,sizeof(bytes));
    assert(output_open(NX_CAT_DEV,1,&handle)==0);
    assert(output_open(NX_CAT_DEV,1,&other)==-NX_ENXIO);
    assert(output_ioctl(handle,0,NX_OUTPUT_ATTACH,0)==0 && pins==1);
    assert(output_image_busy(&inode));
    fail_at=2;
    assert(output_io(handle,0,(uintptr_t)bytes,sizeof(bytes),true)==512);
    assert(!memcmp(capture,bytes,512));
    assert(output_io(handle,0,(uintptr_t)bytes,1,true)==-NX_EIO);
    assert(writes==2);assert(output_close(handle)==0 && pins==0);
    assert(!output_image_busy(&inode));
    assert(output_open(NX_CAT_DEV,1,&handle)==0);
    assert(output_ioctl(handle,0,NX_OUTPUT_ATTACH,0)==0);
    fail_at=writes+1;
    assert(output_io(handle,0,(uintptr_t)bytes,1,true)==-NX_ENOSPC);
    assert(output_close(handle)==0 && pins==0);
    assert(output_open(NX_CAT_DEV,0,&handle)==-NX_ENXIO);
    fail_at=0;
    assert(output_open(NX_VP_DEV,1,&handle)==0);
    assert(output_open(NX_VP_DEV,1,&other)==-NX_ENXIO);
    fail_at=writes+1;
    assert(output_ioctl(handle,0,NX_OUTPUT_ATTACH,0)==-NX_ENOSPC && pins==1);
    assert(output_close(handle)==0 && pins==0);
    fail_at=0;
    assert(output_open(NX_VS_DEV,2,&handle)==0);
    assert(output_ioctl(handle,0,NX_OUTPUT_ATTACH,0)==0);
    assert(output_open(NX_VS_DEV,0,&other)==0);
    assert(output_close(other)==0 && pins==1);
    unsigned char byte;
    assert(output_io(handle,0,(uintptr_t)&byte,1,false)==-NX_EINTR);
    fail_at=writes+1;
    assert(output_close(handle)==-NX_ENOSPC && pins==0);
    puts("PASS: output partial transfer, sticky storage failure, exclusive open and pin release");
}
