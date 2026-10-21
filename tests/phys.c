#define _GNU_SOURCE
#include "phys.h"
#include "accounting.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#define BASE 0x20000000UL
#define COUNT 160
#define SPACES 132
static struct proc p;
static struct user u;
static unsigned char used[COUNT];
struct leaf {uintptr_t pa;unsigned flags;};
static struct leaf leaves[SPACES][16];
static int fail=-1,map_calls,claims,frees,locked;
uintptr_t proc_irq_save(void){assert(!locked);locked=1;return 0x200;}
void proc_irq_restore(uintptr_t f){assert(f==0x200 && locked);locked=0;}
struct proc *proc_current(void){return &p;}
bool proc_superuser(struct user *x){return x->u_uid==0;}
static int frame_index(uintptr_t pa){return pa>=BASE && pa<BASE+COUNT*PAGE_SIZE && !(pa&(PAGE_SIZE-1))?(int)((pa-BASE)/PAGE_SIZE):-1;}
bool page_is_eligible(uintptr_t pa){return frame_index(pa)>=0;}
bool page_is_allocated(uintptr_t pa){int i=frame_index(pa);return i>=0 && used[i];}
bool page_claim(uintptr_t pa){int i=frame_index(pa);if(i<0 || used[i])return false;used[i]=1;++claims;return true;}
bool page_free(uintptr_t pa){int i=frame_index(pa);assert(i>=0 && used[i]);used[i]=0;++frees;return true;}
static unsigned slot(uintptr_t va){assert(va>=NX_PHYS_BASE && va<NX_PHYS_BASE+8*8192 && !(va&4095));return (va-NX_PHYS_BASE)/4096;}
bool vm_translate(const struct address_space *s,uintptr_t va,uintptr_t *pa,unsigned *flags)
{struct leaf *l=&leaves[s->root][slot(va)];if(!l->pa)return false;if(pa)*pa=l->pa;if(flags)*flags=l->flags;return true;}
bool vm_map_physical(struct address_space *s,uintptr_t va,uintptr_t pa)
{struct leaf *l=&leaves[s->root][slot(va)];if(l->pa || (fail>=0 && map_calls++==fail))return false;*l=(struct leaf){pa,VM_USER|VM_WRITE|VM_PHYS};return true;}
bool vm_rebind_physical(struct address_space *s,uintptr_t va,uintptr_t pa)
{struct leaf *l=&leaves[s->root][slot(va)];assert(l->pa && (l->flags&VM_PHYS));l->pa=pa;return true;}
bool vm_unmap(struct address_space *s,uintptr_t va)
{struct leaf *l=&leaves[s->root][slot(va)];assert(l->pa);*l=(struct leaf){0};return true;}
bool vm_walk_user(const struct address_space *s,bool (*visit)(uintptr_t,uintptr_t,unsigned,void *),void *arg)
{for(unsigned i=0;i<16;++i)if(leaves[s->root][i].pa && !visit(NX_PHYS_BASE+i*4096,leaves[s->root][i].pa,leaves[s->root][i].flags,arg))return false;return true;}
_Noreturn void panic(const char *s){fprintf(stderr,"panic: %s\n",s);abort();}
static void space(unsigned n){p.p_vm.root=n;p.p_size=0;}
static void release(unsigned n)
{for(unsigned i=0;i<16;++i)if(leaves[n][i].pa){if(leaves[n][i].flags&VM_PHYS)phys_release(leaves[n][i].pa);leaves[n][i]=(struct leaf){0};}}
int main(void)
{
    void *mem=mmap((void *)BASE,COUNT*PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(mem==(void *)BASE);memset(mem,0xa5,COUNT*PAGE_SIZE);p.p_user=&u;space(1);
    u.u_uid=123;assert(phys_request(0,1,BASE)==-NX_EPERM && !claims);u.u_uid=0;
    assert(phys_request(8,1,BASE)==-NX_EINVAL && phys_request(0,3,BASE)==-NX_EINVAL);
    assert(phys_request(0,1,BASE+1)==-NX_EINVAL && phys_request(0,1,UINTPTR_MAX)==-NX_EINVAL);
    assert(phys_request(0,2,PHYSICAL_LIMIT-PAGE_SIZE)==-NX_EINVAL);
    assert(phys_request(0,1,0x1000)==-NX_ENXIO);
    leaves[1][1]=(struct leaf){BASE,VM_USER|VM_WRITE};assert(phys_request(0,0,0)==-NX_EINVAL);leaves[1][1]=(struct leaf){0};
    used[0]=1;assert(phys_request(0,1,BASE)==-NX_EBUSY);used[0]=0;
    assert(!phys_request(0,1,BASE) && p.p_size==PAGE_SIZE && used[0]);
    for(unsigned i=0;i<PAGE_SIZE;++i)assert(((unsigned char *)mem)[i]==0);
    *(unsigned char *)mem=42;
    assert(!phys_request(1,1,BASE) && p.p_size==2*PAGE_SIZE && *(unsigned char *)mem==42);
    used[3]=1;
    assert(phys_request(0,2,BASE+2*PAGE_SIZE)==-NX_EBUSY && leaves[1][0].pa==BASE && !used[2]);used[3]=0;
    fail=1;map_calls=0;int old_free=frees;
    assert(phys_request(2,2,BASE+2*PAGE_SIZE)==-NX_ENOMEM && !leaves[1][4].pa && !leaves[1][5].pa && frees==old_free+2);
    assert(leaves[1][0].pa==BASE && *(unsigned char *)mem==42);fail=-1;
    assert(!phys_request(0,2,BASE+2*PAGE_SIZE) && leaves[1][0].pa==BASE+2*PAGE_SIZE && used[0]);
    assert(!phys_request(1,0,UINTPTR_MAX) && !used[0]);
    struct address_space dest={.root=2};size_t bytes=0;
    u.u_uid=123;bytes=99;
    assert(!phys_exec(&p.p_vm,&dest,&bytes) && !bytes && !leaves[2][0].pa);
    u.u_uid=0;
    assert(!phys_exec(&p.p_vm,&dest,&bytes) && bytes==8192 && leaves[2][0].pa==leaves[1][0].pa);
    assert(!phys_request(0,0,0) && used[2] && used[3]);release(2);assert(!used[2] && !used[3]);
    assert(!phys_request(0,2,BASE));
    dest.root=3;leaves[3][1]=(struct leaf){BASE+10*PAGE_SIZE,VM_USER|VM_WRITE};
    assert(phys_exec(&p.p_vm,&dest,&bytes)==-NX_EINVAL && leaves[1][0].pa==BASE);
    release(3);
    dest.root=4;fail=1;map_calls=0;assert(phys_exec(&p.p_vm,&dest,&bytes)==-NX_ENOMEM);fail=-1;release(4);
    assert(!phys_request(0,0,0) && !used[0] && !used[1]);
    old_free=frees;assert(!phys_request(0,1,NX_PHYS_VGA_BASE) && !phys_request(0,0,0) && frees==old_free);
    for(unsigned i=0;i<NX_PHYS_FRAMES;++i){space(i+1);assert(!phys_request(0,1,BASE+i*PAGE_SIZE));}
    space(130);assert(phys_request(0,1,BASE+128*PAGE_SIZE)==-NX_ENFILE && !used[128]);
    for(unsigned i=1;i<=NX_PHYS_FRAMES;++i)release(i);
    assert(claims==frees);for(unsigned i=0;i<COUNT;++i)assert(!used[i]);
    assert(!munmap(mem,COUNT*PAGE_SIZE));
    puts("PASS: phys permissions, ownership, zeroing, aliases, capacity, replacement/exec rollback and reference lifetime");return 0;
}
