/* Exercise the production private fio/mpx implementation; section GC removes
 * unrelated filesystem/device paths. QEMU supplies the real syscall/VM tests. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/v7/files.c"
static struct proc process;
static unsigned wakes,sleeps;
static int (*on_sleep)(void *);
static uintptr_t readonly_start,readonly_end;
struct proc *proc_current(void){return &process;}
_Noreturn void panic(const char *message){fprintf(stderr,"panic: %s\n",message);abort();}
unsigned proc_wakeup(void *channel){assert(channel);++wakes;return 1;}
int proc_sleep_interruptible(void *channel,unsigned priority)
{assert(priority==40);++sleeps;return on_sleep?on_sleep(channel):-NX_EINTR;}
bool user_access_ok(const struct address_space *vm,uintptr_t address,size_t length,bool write)
{
    (void)vm;if(!length)return true;
    return address>=4096 && address<=UINTPTR_MAX-length &&
        !(write && address<readonly_end && address+length>readonly_start);
}
int copyin(const struct address_space *vm,uintptr_t source,void *destination,size_t length)
{if(!user_access_ok(vm,source,length,false))return -NX_EFAULT;memcpy(destination,(void *)source,length);return 0;}
int copyout(const struct address_space *vm,uintptr_t destination,const void *source,size_t length)
{if(!user_access_ok(vm,destination,length,true))return -NX_EFAULT;memcpy((void *)destination,source,length);return 0;}
static void reset_mx(void)
{
    memset(mx_groups,0,sizeof(mx_groups));memset(mx_channels,0,sizeof(mx_channels));
    wakes=sleeps=0;on_sleep=NULL;readonly_start=readonly_end=0;
}
static int orphan_waiter(void *channel)
{
    struct mx_channel *c=channel;assert(c->users==1 && c->y.size==200);
    c->closing=true;c->parent->entry[c->index]=(struct mx_entry){0};c->parent=NULL;
    mx_reap(c);assert(c->active && c->users==1);return 0;
}
static int drain_waiter(void *channel)
{
    struct mx_channel *c=channel;assert(c->y.size==200);
    mx_drop(c,&c->y,181);return 0;
}
int main(void)
{
    reset_mx();struct mx_group *g=&mx_groups[0];struct mx_channel *c=mx_allocate(g);assert(c);
    char payload[401];memset(payload,'x',sizeof(payload));
    g->nonblocking=true;
    struct nx_mx_write h={.index=0xfff0,.count=250,.data=payload};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15);
    assert(h.count==50 && h.ccount==-1 && h.data==payload+200 && c->y.size==200);
    mx_drop(c,&c->y,180);assert(c->y.size==20 && !c->unblock_event);
    mx_drop(c,&c->y,1);assert(c->y.size==19 && c->unblock_event && !c->y.blocked_notice);
    uint8_t buffer[256];assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(((struct nx_mx_control *)(buffer+6))->code==NX_M_UBLK && !c->unblock_event);
    mx_drop(c,&c->y,19);
    h.ccount=0;assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16 && c->y.size==50);
    mx_drop(c,&c->y,50);
    /* Every ring position wraps without overwriting unread control records. */
    for(unsigned repeat=0;repeat<3;++repeat){
        for(unsigned i=0;i<NX_MX_EVENTS;++i)assert(mx_event(c,NX_M_EOT,i,NULL));
        assert(!mx_event(c,NX_M_EOT,999,NULL) && c->event_count==NX_MX_EVENTS);
        c->close_event=true;c->unblock_event=true;
        for(unsigned i=0;i<NX_MX_EVENTS;++i){
            assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
            struct nx_mx_control event;memcpy(&event,buffer+6,sizeof(event));assert(event.code==NX_M_EOT && event.value==i);
        }
        assert(mx_group_read(g,(uintptr_t)buffer,12)==10 && ((struct nx_mx_control *)(buffer+6))->code==NX_M_CLOSE);
        assert(mx_group_read(g,(uintptr_t)buffer,12)==10 && ((struct nx_mx_control *)(buffer+6))->code==NX_M_UBLK);
    }
    char name[100];memset(name,'n',sizeof(name));name[99]=0;
    assert(mx_event(c,NX_M_OPEN,123,name));assert(mx_group_read(g,(uintptr_t)buffer,12)==0 && c->event_count==1);
    assert(mx_group_read(g,(uintptr_t)buffer,sizeof(buffer))==60 && !c->event_count);
    assert(((struct nx_mx_read *)buffer)->ccount==54 && buffer[59]==0);
    /* Preflight rejects every descriptor/payload before any queue mutation. */
    struct nx_mx_write records[2]={{.index=0xfff0,.count=3,.data=payload},{.index=0xfff0,.count=1,.data=(void *)1}};
    assert(mx_group_write(g,(uintptr_t)records,sizeof(records))==-NX_EFAULT && c->y.size==0);
    readonly_start=(uintptr_t)&h;readonly_end=readonly_start+sizeof(h);
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==-NX_EFAULT && c->y.size==0);
    readonly_start=readonly_end=0;
    assert(mx_group_write(g,(uintptr_t)&h,15)==-NX_EINVAL);
    /* Arrays larger than the channel table are legal; no native batch cap. */
    struct nx_mx_write many[80];
    for(unsigned i=0;i<80;++i)many[i]=(struct nx_mx_write){.index=0xfff0,.count=1,.data=payload};
    assert(mx_group_write(g,(uintptr_t)many,sizeof(many))==sizeof(many) && c->y.size==80);
    mx_drop(c,&c->y,80);
    g->nonblocking=false;
    h=(struct nx_mx_write){.index=0xfff0,.count=401,.data=payload};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15 && sleeps==1 && h.count==201);
    assert(c->y.size==200 && !c->users);mx_drop(c,&c->y,200);
    h=(struct nx_mx_write){.index=0xfff0,.count=250,.data=payload};on_sleep=drain_waiter;
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16 && c->y.size==69 && !c->users);
    mx_drop(c,&c->y,69);
    /* An in-flight group writer survives detach and cannot hit a reused slot. */
    on_sleep=orphan_waiter;h=(struct nx_mx_write){.index=0xfff0,.count=401,.data=payload};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15 && h.count==201 && !c->active);
    assert(mx_allocate(g)==c && c->y.size==0 && c->event_count==0);
    puts("PASS: mpx record validation, queue bounds/wrap, low-water wakeups, control retention, nonblocking retry, interrupted writes and pinned detach lifetime");return 0;
}
