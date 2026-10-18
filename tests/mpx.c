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
static uint8_t output[256];static unsigned output_count;
void serial_putc(uint8_t c){assert(output_count<sizeof(output));output[output_count++]=c;}
void serial_configure(unsigned rate,unsigned mode){(void)rate;(void)mode;}
void serial_receive_enable(void){}
void signal_group(v7_pid_t group,unsigned sig){(void)group;(void)sig;}
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
    reset_mx();tty_init();struct user user={.u_procp=&process};struct tty *t;
    process.p_pid=17;assert(tty_open(NX_CONSOLE_DEV,&user,&t)==0);
    g=&mx_groups[0];c=mx_allocate(g);assert(c);
    c->input_tty=c->output_tty=t;assert(tty_mpx_bind(t,c)==0);
    struct nx_sgttyb settings;assert(tty_control(t,NX_TIOCGETP,&settings)==0);
    settings.flags=NX_CRMOD;assert(tty_control(t,NX_TIOCSETP,&settings)==0);
    tty_receive('a');tty_receive('\n');
    assert(mx_group_read(g,(uintptr_t)buffer,sizeof(buffer))==8 && !memcmp(buffer+6,"a\n",2));
    tty_receive(4);assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(((struct nx_mx_control *)(buffer+6))->code==NX_M_EOT);
    for(unsigned i=0;i<NX_MX_EVENTS;++i)assert(mx_event(c,NX_M_EOT,0,NULL));
    tty_receive(3);assert(c->signals&(1U<<NX_SIGINT));
    for(unsigned i=0;i<NX_MX_EVENTS;++i)assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    assert(((struct nx_mx_control *)(buffer+6))->code==NX_M_SIG && !c->signals);
    g->nonblocking=true;tty_receive(19);
    h=(struct nx_mx_write){.index=0xfff0,.count=3,.data="OUT"};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==15 && c->tty_blocked && output_count==0);
    tty_receive(17);assert(!c->tty_blocked && c->unblock_event);
    assert(mx_group_read(g,(uintptr_t)buffer,12)==10);
    h.ccount=0;assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16 && output_count==3 && !memcmp(output,"OUT",3));
    struct nx_mx_ioctl request={.code=NX_M_IOCTL,.command=NX_TIOCGETP};
    h=(struct nx_mx_write){.index=0xfff0,.ccount=sizeof(request),.data=&request};
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==16);
    assert(mx_group_read(g,(uintptr_t)buffer,sizeof(buffer))==18 && ((struct nx_mx_ioctl *)(buffer+6))->command==NX_TIOCGETP);
    request.command=NX_MDM_GET;
    assert(mx_group_write(g,(uintptr_t)&h,sizeof(h))==-NX_ENOTTY);
    assert(mx_ioctl_control(c,&request)==-NX_ENOTTY);
    struct file modem_request={.f_private=c,.f_flag=V7_FMPY};
    assert(mx_endpoint_ioctl(&modem_request,&process.p_vm,NX_MDM_GET,(uintptr_t)buffer)==-NX_ENOTTY);
    request.command=NX_TIOCGETP;
    tty_mpx_unbind(t,c);c->input_tty=c->output_tty=NULL;tty_close(t);
    /* A reply remains owned by its sleeping caller until it consumes data. */
    c->ioctl_busy=c->ioctl_pending=true;c->ioctl_command=NX_TIOCGETP;
    assert(mx_ioctl_control(c,&request)==0 && !c->ioctl_pending && c->ioctl_busy);
    c->ioctl_pending=c->ioctl_abandoned=true;
    assert(mx_ioctl_control(c,&request)==-NX_EINTR && !c->ioctl_pending && !c->ioctl_busy);
    c->closing=true;struct file endpoint={.f_private=c,.f_flag=V7_FMPY};
    assert(mx_endpoint_ioctl(&endpoint,&process.p_vm,NX_TIOCGETP,(uintptr_t)&settings)==-NX_ENXIO);
    puts("PASS: mpx record validation, queue bounds/wrap, low-water wakeups, control retention, nonblocking retry, interrupted writes and pinned detach lifetime, TTY data/EOF/signals/flow and ioctl replies");return 0;
}
