/* V7 dev/mem.c: byte-addressed native memory and EOF/rathole devices. */
#include "memdev.h"
#include "accounting.h"
#include "uaccess.h"
#include <nexix/phys.h>
#include <nexix/errno.h>
extern const uint8_t kernel_start[],kernel_end[];
bool memdev_matches(v7_dev_t dev)
{return dev==NX_NULL_DEV || (dev>>8)==NX_MEMORY_MAJOR;}
static bool sink(v7_dev_t dev){return dev==NX_NULL_DEV || dev==NX_MEMORY_NULL_DEV;}
int memdev_open(v7_dev_t dev,struct user *u)
{
    if(sink(dev))return 0;
    if(dev!=NX_MEM_DEV && dev!=NX_KMEM_DEV)return -NX_ENXIO;
    return proc_superuser(u)?0:-NX_EPERM;
}
static int translate(v7_dev_t dev,uintptr_t address,bool write,uintptr_t *physical)
{
    unsigned flags;uintptr_t pa;
    if(address>=PHYSICAL_LIMIT || !vm_translate(paging_kernel_space(),address,&pa,&flags) ||
       (flags&VM_USER) || (dev==NX_MEM_DEV && pa!=address))return -NX_ENXIO;
    uintptr_t page=pa&~(PAGE_SIZE-1);
    /* A present identity PTE alone does not prove firmware RAM backing. */
    bool image=page>=(uintptr_t)kernel_start && page<(uintptr_t)kernel_end;
    bool video=page>=NX_PHYS_VGA_BASE && page<NX_PHYS_VGA_END;
    if(!image && !video && !page_is_eligible(page))return -NX_ENXIO;
    if(write && !(flags&VM_WRITE))return -NX_EACCES;
    *physical=pa;return 0;
}
int64_t memdev_io(v7_dev_t dev,struct user *u,struct address_space *vm,int64_t offset,uintptr_t address,size_t count,bool write)
{
    int error=memdev_open(dev,u);if(error)return error;
    if(count>INT64_MAX)return -NX_EINVAL;
    if(!user_access_ok(vm,address,count,!write))return -NX_EFAULT;
    if(offset<0 || (write && count>(uint64_t)(INT64_MAX-offset)))return -NX_EINVAL;
    if(sink(dev))return write?(int64_t)count:0;
    if(!count)return 0;
    if((uint64_t)offset>=PHYSICAL_LIMIT || count>PHYSICAL_LIMIT-(uint64_t)offset)return -NX_ENXIO;
    /* Preflight all pages: crossing a hole/readonly leaf cannot partly patch RAM. */
    size_t checked=0;
    while(checked<count){
        uintptr_t cursor=(uintptr_t)offset+checked,pa;
        error=translate(dev,cursor,write,&pa);if(error)return error;
        size_t n=PAGE_SIZE-(cursor&(PAGE_SIZE-1));if(n>count-checked)n=count-checked;
        checked+=n;
    }
    /* Byte transfers preserve the original device contract, including VGA.
     * IRQ serialization keeps mappings stable across user-copy operations. */
    size_t done=0;uint8_t bytes[128];
    while(done<count){
        uintptr_t cursor=(uintptr_t)offset+done,pa;
        if(translate(dev,cursor,write,&pa))panic("memory device mapping changed");
        size_t n=PAGE_SIZE-(cursor&(PAGE_SIZE-1));
        if(n>sizeof(bytes))n=sizeof(bytes);
        if(n>count-done)n=count-done;
        if(write){
            if(copyin(vm,address+done,bytes,n))panic("memory device input changed");
            for(size_t i=0;i<n;++i)((volatile uint8_t *)pa)[i]=bytes[i];
        }else{
            for(size_t i=0;i<n;++i)bytes[i]=((volatile uint8_t *)pa)[i];
            if(copyout(vm,address+done,bytes,n))panic("memory device output changed");
        }
        done+=n;
    }
    return count;
}
