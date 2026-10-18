/* V7 dn.c and du.c protocols on bounded native peer-driven endpoints. */
#include "modem.h"
#include "interrupts.h"
#include <nexix/tty.h>
#include "uaccess.h"
#include <nexix/errno.h>
#define HZ 100U
#define PRIORITY 29
static struct dial {
    unsigned opens,signals;
    bool pending,writing;
    uint8_t digit;
    uint64_t pause;
} dial[4];
static struct synchronous {
    struct proc *owner;
    v7_pid_t pid;
    unsigned opens,signals,state,count,sent;
    bool active,busy,expired;
    uint64_t deadline;
    uint8_t bytes[512];
} du;
static char controller;
enum { READ, PWRITE, WRITE };
static int sleep_on(void *p){return proc_sleep_interruptible(p,PRIORITY)?-NX_EINTR:0;}
static void turn(void)
{du.state=READ;du.count=du.sent=0;du.active=du.expired=false;du.deadline=timer_ticks()+10*HZ;proc_wakeup(&du);}
bool modem_matches(uint16_t dev){return (dev>>8)>=27 && (dev>>8)<=29;}
int modem_open(uint16_t dev,struct user *u,void **out)
{
    if((dev>>8)==27){
        unsigned n=dev&255;if(n>=4)return -NX_ENXIO;
        struct dial *d=&dial[n];
        if(d->opens || (d->signals&(NX_DN_POWER|NX_DN_OCCUPIED)))return -NX_ENXIO;
        d->opens=1;d->pending=d->writing=false;d->pause=0;*out=d;return 0;
    }
    if(dev==NX_DU_DEV){
        if(du.opens && (du.owner!=u->u_procp || du.pid!=u->u_procp->p_pid))return -NX_ENXIO;
        if(!du.opens){du.owner=u->u_procp;du.pid=du.owner->p_pid;du.busy=false;turn();}
        ++du.opens;*out=&du;return 0;
    }
    if(dev==NX_MODEMCTL_DEV){if(u->u_uid)return -NX_EPERM;*out=&controller;return 0;}
    return -NX_ENXIO;
}
void modem_close(void *handle)
{
    if(handle==&controller)return;
    if(handle==&du){if(!--du.opens){du.owner=NULL;du.deadline=0;du.active=du.busy=false;du.count=du.sent=0;proc_wakeup(&du);}return;}
    for(unsigned i=0;i<4;++i)if(handle==&dial[i]){
        dial[i].opens=0;dial[i].pending=dial[i].writing=false;dial[i].pause=0;proc_wakeup(handle);return;
    }
    panic("invalid modem close");
}
static int64_t dial_write(struct dial *d,struct address_space *vm,uintptr_t address,size_t count)
{
    if(!count)return 0;
    while(d->writing){int error=sleep_on(d);if(error)return error;}
    d->writing=true;size_t done=0;int error=0;
    while(!(d->signals&(NX_DN_POWER|NX_DN_ABANDON|NX_DN_CONNECTED))){
        if(done<count && (d->signals&NX_DN_READY) && !d->pending && timer_ticks()>=d->pause){
            uint8_t c;if(copyin(vm,address+done,&c,1)){error=-NX_EFAULT;break;}++done;
            if(c=='-')d->pause=timer_ticks()+2*HZ;
            else {d->digit=c;d->pending=true;d->signals&=~NX_DN_READY;}
        }
        error=sleep_on(d);if(error)break;
    }
    if(d->signals&(NX_DN_POWER|NX_DN_ABANDON))error=-NX_EIO;
    d->writing=false;proc_wakeup(d);
    /* DN completion is a call-level result; hardware failure is EIO even
     * after digits were consumed, matching dnwrite's u_error contract. */
    return error?error:(int64_t)done;
}
static int du_ready(void)
{
    if(!(du.signals&NX_MDM_DSR))return -NX_EIO;
    return 0;
}
static int64_t sync_io(struct address_space *vm,uintptr_t address,size_t count,bool write)
{
    if(!count)return 0;
    while(du.busy){int e=sleep_on(&du);if(e)return e;}
    du.busy=true;int error=0;size_t n=0;
    for(;;){
        error=du_ready();if(error)break;
        if(du.state==READ && !du.active && (write || du.count || du.expired))break;
        error=sleep_on(&du);if(error)break;
    }
    if(!error){
        n=count>512?512:count;
        if(write){
            uint8_t bytes[512];error=copyin(vm,address,bytes,n);
            if(!error){
                for(size_t i=0;i<n;++i)du.bytes[i]=bytes[i];
                du.count=n;du.sent=0;du.state=PWRITE;du.deadline=timer_ticks()+10*HZ;
                while(!(du.signals&NX_MDM_CTS)){
                    error=du_ready();if(error || du.state!=PWRITE){if(!error)error=-NX_EIO;break;}
                    error=sleep_on(&du);if(error)break;
                }
                if(!error && (du.state!=PWRITE || du_ready()))error=-NX_EIO;
                if(!error){du.state=WRITE;du.deadline=timer_ticks()+10*HZ;proc_wakeup(&du);}
                else turn();
            }
        }else{
            if(n>du.count)n=du.count;
            error=copyout(vm,address,du.bytes,n);
            /* V7 duread does not consume the buffered record. */
        }
    }
    du.busy=false;proc_wakeup(&du);return error?error:(int64_t)n;
}
int64_t modem_io(void *handle,struct address_space *vm,uintptr_t address,size_t count,bool write)
{
    if(handle==&controller)return -NX_ENXIO;
    if(handle==&du)return sync_io(vm,address,count,write);
    for(unsigned i=0;i<4;++i)if(handle==&dial[i])return write?dial_write(handle,vm,address,count):-NX_ENXIO;
    return -NX_ENXIO;
}
static int peer(struct nx_modem_peer *p)
{
    if(p->reserved || p->count>512)return -NX_EINVAL;
    if(p->operation==NX_PEER_TTY){
        if(p->device!=NX_CONSOLE_DEV || p->count || (p->signals&~31U))return -NX_EINVAL;
        return tty_modem_peer(p->signals);
    }
    if((p->device>>8)==27 && (p->device&255)<4){
        struct dial *d=&dial[p->device&255];
        if(p->operation==NX_PEER_SIGNALS){if(p->signals&~31U || p->count)return -NX_EINVAL;d->signals=p->signals;}
        else if(p->operation==NX_PEER_TAKE){
            if(!p->count)return -NX_EINVAL;
            p->count=d->pending?1:0;if(d->pending){p->data[0]=d->digit;d->pending=false;}
        }else if(p->operation!=NX_PEER_STATUS)return -NX_EINVAL;
        p->signals=d->signals;p->state=(d->opens?NX_MODEM_OPEN:0)|(d->pending?NX_MODEM_PENDING:0);
        proc_wakeup(d);return 0;
    }
    if(p->device!=NX_DU_DEV)return -NX_ENXIO;
    switch(p->operation){
    case NX_PEER_SIGNALS:
        if(p->signals&~15U || p->count)return -NX_EINVAL;
        if((du.signals&NX_MDM_DCD) && !(p->signals&NX_MDM_DCD) && du.state==READ)du.active=false;
        du.signals=p->signals;break;
    case NX_PEER_RECEIVE:
        if(!du.opens || du.state!=READ || !(du.signals&NX_MDM_DSR))return -NX_EIO;
        if(p->count>512-du.count)return -NX_ENOSPC;
        for(unsigned i=0;i<p->count;++i)du.bytes[du.count++]=p->data[i];
        du.active=true;du.expired=false;break;
    case NX_PEER_END:
        if(!du.opens || du.state!=READ || p->count)return -NX_EINVAL;
        du.active=false;break;
    case NX_PEER_TAKE:{
        if(!p->count)return -NX_EINVAL;
        if(!du.opens || du.state!=WRITE || !(du.signals&NX_MDM_CTS))return -NX_EAGAIN;
        unsigned n=du.count-du.sent;if(n>p->count)n=p->count;
        for(unsigned i=0;i<n;++i)p->data[i]=du.bytes[du.sent++];
        p->count=n;du.deadline=timer_ticks()+10*HZ;
        if(du.sent==du.count)turn();
        break;
    }
    case NX_PEER_STATUS:break;
    default:return -NX_EINVAL;
    }
    p->signals=du.signals;p->state=(du.opens?NX_MODEM_OPEN:0)|
        (du.state==READ?NX_MODEM_READ:du.state==WRITE?NX_MODEM_WRITE:NX_MODEM_PENDING)|
        (du.opens?NX_MDM_DTR:0)|(du.opens && du.state!=READ?NX_MDM_RTS:0);
    proc_wakeup(&du);return 0;
}
int modem_ioctl(void *handle,struct address_space *vm,uint64_t request,uintptr_t address)
{
    if(handle!=&controller || request!=NX_MDM_PEER)return -NX_ENOTTY;
    struct proc *p=proc_current();if(!p || p->p_user->u_uid)return -NX_EPERM;
    /* Preflight the entire in/out record before any destructive TAKE. */
    if(!user_access_ok(vm,address,sizeof(struct nx_modem_peer),true))return -NX_EFAULT;
    struct nx_modem_peer data;int error=copyin(vm,address,&data,sizeof(data));
    if(!error)error=peer(&data);
    if(!error)error=copyout(vm,address,&data,sizeof(data));
    return error;
}
void modem_timer(uint64_t now)
{
    for(unsigned i=0;i<4;++i)if(dial[i].opens && dial[i].pause && now>=dial[i].pause){dial[i].pause=0;proc_wakeup(&dial[i]);}
    if(du.opens && du.deadline && now>=du.deadline){
        if(du.state!=READ)turn();else {du.active=false;du.expired=true;du.deadline=now+HZ;proc_wakeup(&du);}
    }
}
