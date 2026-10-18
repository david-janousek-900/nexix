#define _GNU_SOURCE
#include "memdev.h"
#include "accounting.h"
#include "uaccess.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#define BASE 0x20000000UL
#define ALIAS 0x30000000UL
__asm__(".global kernel_start\n.set kernel_start,0x100000\n.global kernel_end\n.set kernel_end,0x200000\n");
static struct address_space kernel={1},vm={2};
static struct user u;
static bool deny,ro;
static unsigned copies;
const struct address_space *paging_kernel_space(void){return &kernel;}
bool page_is_eligible(uintptr_t pa){return pa>=BASE && pa<BASE+3*PAGE_SIZE;}
bool proc_superuser(struct user *p){return p->u_uid==0;}
bool vm_translate(const struct address_space *s,uintptr_t va,uintptr_t *pa,unsigned *flags)
{
    assert(s==&kernel);
    if(va>=BASE && va<BASE+4*PAGE_SIZE){*pa=va;*flags=ro && va>=BASE+PAGE_SIZE?0:VM_WRITE;return true;}
    if(va>=ALIAS && va<ALIAS+PAGE_SIZE){*pa=BASE+va-ALIAS;*flags=VM_WRITE;return true;}
    if(va>=0x100000 && va<0x101000){*pa=va;*flags=0;return true;}
    return false;
}
bool user_access_ok(const struct address_space *s,uintptr_t a,size_t n,bool w)
{(void)w;assert(s==&vm);return !n || (!deny && a>=4096 && a<=UINTPTR_MAX-n);}
int copyin(const struct address_space *s,uintptr_t a,void *b,size_t n)
{assert(user_access_ok(s,a,n,false));memcpy(b,(void *)a,n);++copies;return 0;}
int copyout(const struct address_space *s,uintptr_t a,const void *b,size_t n)
{assert(user_access_ok(s,a,n,true));memcpy((void *)a,b,n);++copies;return 0;}
_Noreturn void panic(const char *s){fprintf(stderr,"panic %s\n",s);abort();}
static int64_t io(int dev,int64_t offset,void *bytes,size_t n,bool write)
{return memdev_io(dev,&u,&vm,offset,(uintptr_t)bytes,n,write);}
int main(void)
{
    uint8_t *ram=mmap((void *)BASE,4*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(ram==(void *)BASE);
    uint8_t *image=mmap((void *)0x100000,PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(image==(void *)0x100000);image[0]=0x42;
    uint8_t bytes[5000],before[5000];for(unsigned i=0;i<sizeof(bytes);++i)bytes[i]=i*29;
    assert(memdev_matches(NX_NULL_DEV) && memdev_matches(NX_MEM_DEV) && memdev_matches(0x6ff) && !memdev_matches(0x700));
    assert(memdev_open(0x603,&u)==-NX_ENXIO);
    assert(io(NX_MEM_DEV,BASE+3,bytes,sizeof(bytes),true)==sizeof(bytes));
    memset(before,0,sizeof(before));assert(io(NX_KMEM_DEV,BASE+3,before,sizeof(before),false)==sizeof(before));
    assert(!memcmp(bytes,before,sizeof(bytes)));
    assert(io(NX_KMEM_DEV,ALIAS+3,before,32,false)==32 && !memcmp(bytes,before,32));
    assert(io(NX_MEM_DEV,ALIAS,bytes,1,false)==-NX_ENXIO); /* Physical addresses cannot silently use an alias. */
    unsigned old=copies;ro=true;
    assert(io(NX_MEM_DEV,BASE+PAGE_SIZE-1,bytes,2,true)==-NX_EACCES && copies==old);
    ro=false;assert(io(NX_MEM_DEV,BASE+3*PAGE_SIZE-1,bytes,2,true)==-NX_ENXIO && copies==old);
    memcpy(before,bytes,sizeof(bytes));
    assert(io(NX_MEM_DEV,BASE+3*PAGE_SIZE-1,bytes,2,false)==-NX_ENXIO && copies==old && !memcmp(before,bytes,sizeof(bytes)));
    assert(io(NX_MEM_DEV,0,bytes,1,false)==-NX_ENXIO);
    assert(io(NX_MEM_DEV,PHYSICAL_LIMIT-1,bytes,2,false)==-NX_ENXIO);
    assert(io(NX_MEM_DEV,INT64_MAX,bytes,1,false)==-NX_ENXIO);
    assert(io(NX_MEM_DEV,-1,bytes,1,false)==-NX_EINVAL);
    assert(io(NX_MEM_DEV,BASE,bytes,SIZE_MAX,true)==-NX_EINVAL);
    assert(io(NX_MEM_DEV,0x100000,bytes,1,false)==1 && bytes[0]==0x42);
    assert(io(NX_KMEM_DEV,0x100000,bytes,1,true)==-NX_EACCES && image[0]==0x42);
    deny=true;assert(io(NX_MEM_DEV,BASE,bytes,1,true)==-NX_EFAULT);deny=false;
    assert(io(NX_MEM_DEV,0,NULL,0,false)==0);
    for(unsigned i=0;i<2;++i){int dev=i?NX_MEMORY_NULL_DEV:NX_NULL_DEV;
        assert(io(dev,17,bytes,33,true)==33 && io(dev,17,bytes,33,false)==0);
        assert(io(dev,INT64_MAX,bytes,1,true)==-NX_EINVAL);
    }
    u.u_uid=100;assert(memdev_open(NX_MEM_DEV,&u)==-NX_EPERM && memdev_open(NX_KMEM_DEV,&u)==-NX_EPERM);
    assert(io(NX_MEM_DEV,BASE,bytes,1,false)==-NX_EPERM && io(NX_KMEM_DEV,BASE,bytes,1,true)==-NX_EPERM);
    assert(io(NX_NULL_DEV,0,bytes,1,true)==1 && memdev_open(NX_MEMORY_NULL_DEV,&u)==0);
    assert(!munmap(ram,4*PAGE_SIZE) && !munmap(image,PAGE_SIZE));
    puts("PASS: memory devices, privilege, translated aliases, backing/permission preflight, holes/bounds, user-copy and null semantics");
}
