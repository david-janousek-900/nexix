/* V7 sysphys/ABS mappings, adapted to fixed native windows and owned frames. */
#include "phys.h"
#include "accounting.h"
#include <nexix/errno.h>
struct frame {uintptr_t pa;uint32_t refs;bool ram;};
static struct frame frames[NX_PHYS_FRAMES];
static int acquire(uintptr_t pa)
{
    struct frame *slot=NULL;
    for(unsigned i=0;i<NX_PHYS_FRAMES;++i){
        if(frames[i].refs && frames[i].pa==pa){
            if(frames[i].refs==UINT32_MAX)return -NX_ENOMEM;
            ++frames[i].refs;return 0;
        }
        if(!frames[i].refs && !slot)slot=&frames[i];
    }
    bool ram=!(pa>=NX_PHYS_VGA_BASE && pa<NX_PHYS_VGA_END);
    if(ram && !page_is_eligible(pa))return -NX_ENXIO;
    if(ram && page_is_allocated(pa))return -NX_EBUSY;
    if(!slot)return -NX_ENFILE;
    if(ram){
        if(!page_claim(pa))return -NX_EBUSY;
        /* A newly claimed frame must not expose the previous owner's bytes. */
        for(size_t i=0;i<PAGE_SIZE;++i)((volatile uint8_t *)pa)[i]=0;
    }
    *slot=(struct frame){pa,1,ram};return 0;
}
void phys_release(uintptr_t pa)
{
    for(unsigned i=0;i<NX_PHYS_FRAMES;++i)if(frames[i].refs && frames[i].pa==pa){
        if(!--frames[i].refs){
            if(frames[i].ram && !page_free(pa))panic("physical frame release");
            frames[i]=(struct frame){0};
        }
        return;
    }
    panic("unowned physical frame");
}
bool phys_clone_leaf(struct address_space *vm,uintptr_t va,uintptr_t pa)
{
    /* Only existing mappings can be inherited; never acquire arbitrary bytes. */
    bool known=false;
    for(unsigned i=0;i<NX_PHYS_FRAMES;++i)if(frames[i].refs && frames[i].pa==pa)known=true;
    if(!known)panic("physical clone ownership");
    if(acquire(pa))return false;
    if(!vm_map_physical(vm,va,pa)){phys_release(pa);return false;}
    return true;
}
struct clone {struct address_space *vm;size_t bytes;int error;};
static bool clone_one(uintptr_t va,uintptr_t pa,unsigned flags,void *argument)
{
    struct clone *c=argument;
    if(!(flags&VM_PHYS))return true;
    if(vm_translate(c->vm,va,NULL,NULL)){c->error=-NX_EINVAL;return false;}
    if(!phys_clone_leaf(c->vm,va,pa)){c->error=-NX_ENOMEM;return false;}
    c->bytes+=PAGE_SIZE;return true;
}
int phys_exec(const struct address_space *old,struct address_space *next,size_t *bytes)
{
    /* Physical windows are a root capability, not ordinary exec data. */
    if(proc_current()->p_user->u_uid){*bytes=0;return 0;}
    struct clone c={.vm=next};
    if(!vm_walk_user(old,clone_one,&c))return c.error?c.error:-NX_EINVAL;
    *bytes=c.bytes;return 0;
}
int phys_request(uint64_t slot,uint64_t pages,uintptr_t physical)
{
    uintptr_t irq=proc_irq_save();struct proc *p=proc_current();int error=0;
    if(!p || !p->p_user || !proc_superuser(p->p_user)){error=-NX_EPERM;goto done;}
    if(slot>=NX_PHYS_WINDOWS || pages>NX_PHYS_PAGES ||
       (pages && (!physical || (physical&(PAGE_SIZE-1)) || physical>=PHYSICAL_LIMIT ||
                  pages>(PHYSICAL_LIMIT-physical)/PAGE_SIZE))){error=-NX_EINVAL;goto done;}
    uintptr_t base=NX_PHYS_ADDRESS(slot),old[NX_PHYS_PAGES]={0};
    bool staged[NX_PHYS_PAGES]={0};unsigned old_count=0,held=0;
    /* A whole window must be free or previously installed by phys. */
    for(unsigned i=0;i<NX_PHYS_PAGES;++i){unsigned flags;
        if(vm_translate(&p->p_vm,base+i*PAGE_SIZE,&old[i],&flags)){
            if(!(flags&VM_PHYS)){error=-NX_EINVAL;goto done;}
            ++old_count;
        }
    }
    for(;held<pages;++held){error=acquire(physical+held*PAGE_SIZE);if(error)goto rollback;}
    /* Stage missing leaves before changing any old leaf. Old entries keep their
     * tables alive, so subsequent rebind/removal cannot fail or allocate. */
    for(unsigned i=0;i<pages;++i)if(!old[i]){
        if(!vm_map_physical(&p->p_vm,base+i*PAGE_SIZE,physical+i*PAGE_SIZE)){
            error=-NX_ENOMEM;goto rollback;
        }
        staged[i]=true;
    }
    for(unsigned i=0;i<NX_PHYS_PAGES;++i)if(old[i]){
        if(i<pages){if(!vm_rebind_physical(&p->p_vm,base+i*PAGE_SIZE,physical+i*PAGE_SIZE))panic("physical rebind");}
        else if(!vm_unmap(&p->p_vm,base+i*PAGE_SIZE))panic("physical unmap");
        phys_release(old[i]);
    }
    p->p_size=p->p_size-old_count*PAGE_SIZE+pages*PAGE_SIZE;
    goto done;
rollback:
    for(unsigned i=0;i<NX_PHYS_PAGES;++i)if(staged[i] && !vm_unmap(&p->p_vm,base+i*PAGE_SIZE))panic("physical rollback");
    while(held)phys_release(physical+--held*PAGE_SIZE);
done:
    proc_irq_restore(irq);return error;
}
