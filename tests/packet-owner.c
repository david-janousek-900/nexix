/* Exercise the production ownership adapter with deterministic IRQ/sleep peers. */
#include "packet.h"
#include "tty.h"
#include "memory.h"
#include "uaccess.h"
#include "interrupts.h"
#include "signals.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nexix/errno.h>
static unsigned pages,peak,wakes,sigpipe,changes,sleeps;
static uint64_t ticks;
static bool nomem,interrupt,disconnect,silent,congested,unblock,idle_context;
static struct packet peer;
static struct tty *terminal;
static uint8_t toward_peer[16384],toward_kernel[16384];
static size_t peer_count,kernel_count;
static struct proc process;
uintptr_t page_alloc(void)
{if(nomem)return 0;void *p=aligned_alloc(PAGE_SIZE,PAGE_SIZE);assert(p);++pages;if(pages>peak)peak=pages;return (uintptr_t)p;}
bool page_free(uintptr_t p){assert(pages);--pages;free((void *)p);return true;}
uint64_t timer_ticks(void){return ticks;}
struct proc *proc_current(void){return idle_context?NULL:&process;}
void signal_post(struct proc *p,unsigned sig){assert(p==&process && sig==NX_SIGPIPE);++sigpipe;}
unsigned proc_wakeup(void *p){assert(p);++wakes;return 0;}
bool tty_packet_reader(struct tty *t){assert(t==terminal);return true;}
void tty_packet_notice(struct tty *t){assert(t==terminal);}
void mpx_tty_discipline(struct tty *t){assert(t==terminal);++changes;}
bool tty_packet_emit(struct tty *t,const uint8_t *b,size_t n)
{assert(t==terminal && peer_count+n<=sizeof(toward_peer));if(congested)return false;memcpy(toward_peer+peer_count,b,n);peer_count+=n;return true;}
static bool emit(void *v,const uint8_t *b,size_t n)
{(void)v;assert(kernel_count+n<=sizeof(toward_kernel));memcpy(toward_kernel+kernel_count,b,n);kernel_count+=n;return true;}
static void pump(void)
{
    for(unsigned step=0;peer_count || kernel_count;++step){
        assert(step<32);size_t n=peer_count;peer_count=0;
        for(size_t i=0;i<n;++i)pk_receive(&peer,toward_peer[i],ticks);
        n=kernel_count;kernel_count=0;
        for(size_t i=0;i<n;++i)packet_receive(terminal,toward_kernel[i]);
    }
}
int proc_sleep_interruptible(void *channel,unsigned priority)
{
    assert(channel && (priority==28 || priority==29));
    assert(!idle_context);++sleeps;
    if(interrupt){interrupt=false;return 1;}
    if(disconnect){disconnect=false;packet_abort(terminal);return 0;}
    ticks+=100;if(unblock){congested=false;unblock=false;}
    if(!silent){pump();uint8_t bytes[512];bool eof;(void)pk_take(&peer,bytes,sizeof(bytes),&eof);pump();}
    packet_timer(ticks);return 0;
}
int copyin(const struct address_space *s,uintptr_t p,void *b,size_t n)
{(void)s;if(n && p<4096)return -NX_EFAULT;if(n)memcpy(b,(void *)p,n);return 0;}
int copyout(const struct address_space *s,uintptr_t p,const void *b,size_t n)
{(void)s;if(n && p<4096)return -NX_EFAULT;if(n)memcpy((void *)p,b,n);return 0;}
_Noreturn void panic(const char *s){fprintf(stderr,"panic %s\n",s);abort();}
static void reset(void)
{
    assert(!pages);peer_count=kernel_count=0;ticks=0;silent=false;nomem=false;interrupt=false;disconnect=false;congested=false;unblock=false;
    terminal=(struct tty *)0x1234;
    pk_initialize(&peer,64,2,emit,NULL,NULL,0);
}
static struct nx_piocb parameters(void)
{return (struct nx_piocb){.discipline=1,.psize=128,.window=2};}
int main(void)
{
    reset();struct nx_piocb p=parameters();nomem=true;assert(packet_open(terminal,&p)==-NX_ENOMEM && !pages);nomem=false;
    p.psize=96;assert(packet_open(terminal,&p)==-NX_EINVAL && !pages);p=parameters();
    interrupt=true;assert(packet_open(terminal,&p)==-NX_EINTR && !packet_active(terminal) && !pages);
    reset();p=parameters();silent=true;assert(packet_open(terminal,&p)==-NX_ENXIO && !pages);silent=false;
    reset();p=parameters();assert(packet_open(terminal,&p)==0 && pages==1 && p.psize==64);
    assert(packet_open(terminal,&p)==-NX_EBUSY);uint8_t bytes[128];bool eof;
    assert(packet_io(terminal,NULL,0,0,false)==0);
    assert(packet_io(terminal,NULL,0,0,true)==0);pump();assert(peer.rx_count==1);
    assert(pk_take(&peer,bytes,sizeof(bytes),&eof)==0 && eof);pump();
    assert(pk_send(&peer,(const uint8_t *)"hello",5,ticks)==0);pump();
    assert(packet_ready(terminal));assert(packet_io(terminal,NULL,(uintptr_t)bytes,sizeof(bytes),false)==5 && !memcmp(bytes,"hello",5));pump();
    assert(packet_io(terminal,NULL,1,1,true)==-NX_EFAULT);
    assert(!pk_send(&peer,(const uint8_t *)"x",1,ticks));pump();
    assert(packet_io(terminal,NULL,1,1,false)==-NX_EFAULT);pump();
    pk_shutdown(&peer);pump();assert(packet_io(terminal,NULL,(uintptr_t)bytes,1,false)==0);
    assert(packet_io(terminal,NULL,(uintptr_t)bytes,1,false)==0); /* Repeated EOF cannot sleep. */
    assert(packet_io(terminal,NULL,(uintptr_t)bytes,1,true)==-NX_EPIPE && sigpipe==1);
    assert(packet_put(terminal,bytes,1)==-NX_EPIPE && sigpipe==2);
    packet_close(terminal);assert(!packet_active(terminal) && !pages);
    reset();p=parameters();assert(!packet_open(terminal,&p));
    interrupt=true;assert(packet_io(terminal,NULL,(uintptr_t)bytes,1,false)==-NX_EINTR && pages==1);
    /* Disable while the reader sleeps: its pin owns the page until it returns. */
    disconnect=true;assert(packet_io(terminal,NULL,(uintptr_t)bytes,1,false)==-NX_EIO && !pages);
    reset();p=parameters();assert(!packet_open(terminal,&p));
    assert(packet_io(terminal,NULL,(uintptr_t)"drain",5,true)==5);
    packet_close(terminal);assert(!pages && !packet_active(terminal));
    reset();p=parameters();assert(!packet_open(terminal,&p));
    congested=true;unblock=true;packet_close(terminal);assert(!pages && !congested);
    reset();p=parameters();assert(!packet_open(terminal,&p));
    congested=true;uint64_t before=ticks;packet_close(terminal);assert(!pages && ticks-before==100);
    reset();p=parameters();assert(!packet_open(terminal,&p));
    assert(packet_put(terminal,(const uint8_t *)"lost",4)==4);silent=true;before=ticks;
    packet_close(terminal);assert(!pages && ticks-before>=PK_RETRY*PK_RETRIES);
    reset();p=parameters();assert(!packet_open(terminal,&p));
    assert(packet_put(terminal,(const uint8_t *)"pending",7)==7);
    congested=true;idle_context=true;unsigned slept=sleeps;
    packet_abort(terminal);
    assert(!packet_active(terminal) && !pages && sleeps==slept);
    packet_abort(terminal);idle_context=false;
    assert(peak==1 && wakes && changes>=8);
    puts("PASS: packet ownership, ENOMEM, invalid open, EINTR, startup timeout, pinned detach, repeated EOF, SIGPIPE and close drain");
}
