/* V7 pk1/pk3 ownership, sleep channels and fixed-size receive/transmit pools. */
#include "packet.h"
#include "tty.h"
#include "memory.h"
#include "interrupts.h"
#include "uaccess.h"
#include "signals.h"
#include <nexix/errno.h>
struct binding {struct packet packet;struct tty *tty;unsigned users;bool attached;};
_Static_assert(sizeof(struct binding)<=PAGE_SIZE,"packet binding exceeds one page");
static struct binding *lines[NX_PK_LINES];
static struct binding *find(struct tty *t)
{for(unsigned i=0;i<NX_PK_LINES;++i)if(lines[i] && lines[i]->tty==t)return lines[i];return NULL;}
static void release(struct binding *b)
{if(!--b->users && !b->attached && !page_free((uintptr_t)b))panic("packet page release");}
static bool emit(void *v,const uint8_t *bytes,size_t n)
{struct binding *b=v;return tty_packet_emit(b->tty,bytes,n);}
static void notify(void *v)
{struct binding *b=v;proc_wakeup(b);tty_packet_notice(b->tty);}
static void detach(struct binding *b)
{
    if(!b->attached)return;
    for(unsigned i=0;i<NX_PK_LINES;++i)if(lines[i]==b)lines[i]=NULL;
    b->attached=false;
    if(b->packet.state!=NX_PK_DOWN)pk_shutdown(&b->packet);
    mpx_tty_discipline(b->tty);notify(b);
}
bool packet_active(struct tty *t){return find(t)!=NULL;}
int packet_status(struct tty *t,struct nx_piocb *p)
{
    struct binding *b=find(t);if(!b)return -NX_ENXIO;
    *p=(struct nx_piocb){.discipline=1,.psize=b->packet.xsize,.state=b->packet.state,.window=b->packet.swindow};
    return p->psize;
}
int packet_open(struct tty *t,struct nx_piocb *p)
{
    if(!pk_size_valid(p->psize) || !p->window || p->window>NX_PK_WINDOW || p->mode<0 || p->mode>1 || p->reserved)return -NX_EINVAL;
    if(find(t))return -NX_EBUSY;
    unsigned slot=0;while(slot<NX_PK_LINES && lines[slot])++slot;
    if(slot==NX_PK_LINES)return -NX_ENXIO;
    struct binding *b=(void *)page_alloc();if(!b)return -NX_ENOMEM;
    uint8_t *raw=(void *)b;for(unsigned i=0;i<PAGE_SIZE;++i)raw[i]=0;
    b->tty=t;b->attached=true;b->users=1;lines[slot]=b;mpx_tty_discipline(t);
    pk_initialize(&b->packet,p->psize,p->window,emit,notify,b,timer_ticks());
    tty_packet_notice(t);
    int error=0;
    while(b->attached && b->packet.state!=NX_PK_LIVE && b->packet.state!=NX_PK_DOWN)
        if(proc_sleep_interruptible(b,28)){error=-NX_EINTR;break;}
    if(!b->attached || b->packet.state!=NX_PK_LIVE){if(!error)error=-NX_ENXIO;}
    if(error)detach(b);else (void)packet_status(t,p);
    release(b);return error;
}
void packet_close(struct tty *t)
{
    struct binding *b=find(t);if(!b)return;
    ++b->users;
    /* Retransmission timer bounds the drain, including an absent peer. */
    while(b->attached && b->packet.state==NX_PK_LIVE && b->packet.tx_count)
        if(proc_sleep_interruptible(b,29))break;
    if(b->attached && b->packet.state!=NX_PK_DOWN)pk_shutdown(&b->packet);
    uint64_t deadline=timer_ticks()+100;
    while(b->attached && b->packet.pending && timer_ticks()<deadline)
        if(proc_sleep_interruptible(b,29))break;
    detach(b);release(b);
}
void packet_abort(struct tty *t)
{
    struct binding *b=find(t);if(!b)return;
    ++b->users;detach(b);release(b);
}
void packet_changed(struct tty *t)
{struct binding *b=find(t);if(b)proc_wakeup(b);}
void packet_receive(struct tty *t,uint8_t byte)
{struct binding *b=find(t);if(b)pk_receive(&b->packet,byte,timer_ticks());}
void packet_timer(uint64_t now)
{for(unsigned i=0;i<NX_PK_LINES;++i)if(lines[i]){
    pk_tick(&lines[i]->packet,now);
    if(lines[i]->packet.state==NX_PK_DOWN)proc_wakeup(lines[i]);
}}
bool packet_ready(struct tty *t)
{struct binding *b=find(t);return b && pk_readable(&b->packet);}
size_t packet_take(struct tty *t,uint8_t *bytes,size_t n,bool *eof)
{struct binding *b=find(t);*eof=false;return b?pk_take(&b->packet,bytes,n,eof):0;}
int64_t packet_put(struct tty *t,const uint8_t *bytes,size_t n)
{
    struct binding *b=find(t);if(!b)return -NX_ENXIO;
    if(n>b->packet.xsize)n=b->packet.xsize;
    int error=pk_send(&b->packet,bytes,n,timer_ticks());
    if(error==-NX_EIO){signal_post(proc_current(),NX_SIGPIPE);return -NX_EPIPE;}
    return error==-NX_EAGAIN?0:error?error:(int64_t)n;
}
int64_t packet_io(struct tty *t,struct address_space *vm,uintptr_t address,size_t count,bool write)
{
    struct binding *b=find(t);if(!b)return -NX_ENXIO;
    ++b->users;size_t done=0;int error=0;uint8_t bytes[NX_PK_MAX_SIZE];
    if(!write){
        if(!count){release(b);return 0;}
        while(b->attached && tty_packet_reader(t) && b->packet.state!=NX_PK_DOWN && !pk_readable(&b->packet))
            if(proc_sleep_interruptible(b,28)){error=-NX_EINTR;break;}
        if(!b->attached)error=-NX_EIO;
        else if(!tty_packet_reader(t))error=-NX_ENXIO;
        if(!error){bool eof;size_t n=count>sizeof(bytes)?sizeof(bytes):count;
            done=pk_take(&b->packet,bytes,n,&eof);
            error=copyout(vm,address,bytes,done);if(error)done=0;}
    }else do{
        while(b->attached && b->packet.state==NX_PK_LIVE && b->packet.tx_count>=b->packet.swindow)
            if(proc_sleep_interruptible(b,29)){error=-NX_EINTR;break;}
        if(error)break;
        if(!b->attached || b->packet.state!=NX_PK_LIVE){error=-NX_EPIPE;signal_post(proc_current(),NX_SIGPIPE);break;}
        size_t n=count-done;if(n>b->packet.xsize)n=b->packet.xsize;
        error=copyin(vm,address+done,bytes,n);if(error)break;
        error=pk_send(&b->packet,bytes,n,timer_ticks());if(error)break;
        done+=n;
    }while(done<count);
    release(b);return done?(int64_t)done:error;
}
