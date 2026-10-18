/* Real packet state machines; delayed transport prevents callback reentrancy. */
#include "packet.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct endpoint {struct packet p;uint8_t wire[65536];size_t used;bool blocked;unsigned notices;};
static struct endpoint a,b;
static uint64_t now;
static bool emit(void *v,const uint8_t *bytes,size_t n)
{struct endpoint *e=v;if(e->blocked)return false;assert(e->used+n<=sizeof(e->wire));memcpy(e->wire+e->used,bytes,n);e->used+=n;return true;}
static void notice(void *v){++((struct endpoint *)v)->notices;}
static void deliver(struct endpoint *from,struct endpoint *to)
{size_t n=from->used;from->used=0;for(size_t i=0;i<n;++i)pk_receive(&to->p,from->wire[i],now);}
static void pump(void)
{for(unsigned n=0;a.used || b.used;++n){assert(n<30);deliver(&a,&b);deliver(&b,&a);}}
static void start(unsigned size,unsigned window)
{
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));now=0;
    pk_initialize(&a.p,size,window,emit,notice,&a,now);
    pk_initialize(&b.p,size,window,emit,notice,&b,now);
    pump();assert(a.p.state==NX_PK_LIVE && b.p.state==NX_PK_LIVE);
}
static void control(struct endpoint *e,unsigned c)
{
    uint16_t sum=NX_PK_CHECK-c;uint8_t h[]={16,9,sum,sum>>8,c,9^(sum&255)^(sum>>8)^c};
    for(unsigned i=0;i<6;++i)pk_receive(&e->p,h[i],now);
}
static void exchange(unsigned size,unsigned window)
{
    start(size,window);uint8_t input[512],out[512];bool eof;
    for(unsigned i=0;i<sizeof(input);++i)input[i]=i*17;
    for(unsigned round=0;round<24;++round){
        unsigned length=round==0?0:round==1?size:round==2?size-1:(round*31)%size;
        assert(pk_send(&a.p,input,length,now)==0);pump();
        assert(a.p.tx_count==1); /* Receipt alone must not acknowledge unread data. */
        size_t half=length/2;
        if(half){assert(pk_take(&b.p,out,half,&eof)==half && !eof);pump();assert(a.p.tx_count==1);}
        assert(pk_take(&b.p,out+half,sizeof(out)-half,&eof)==length-half);
        assert(eof==!length && !memcmp(input,out,length));pump();assert(a.p.tx_count==0);
    }
    for(unsigned i=0;i<window;++i)assert(pk_send(&a.p,input,7,now)==0);
    assert(pk_send(&a.p,input,7,now)==-NX_EAGAIN);pump();
    for(unsigned i=0;i<window;++i){assert(pk_take(&b.p,out,7,&eof)==7 && !eof);pump();}
    pk_shutdown(&a.p);pump();assert(b.p.state==NX_PK_DOWN);
    assert(pk_take(&b.p,out,1,&eof)==0 && eof && !pk_readable(&b.p));
    assert(pk_send(&b.p,input,1,now)==-NX_EIO);
}
int main(void)
{
    uint8_t input[512]={0},out[512];bool eof;
    assert(pk_checksum(input,32)==0x7015); /* Fixed independently evaluated V7 vectors. */
    for(unsigned i=0;i<512;++i)input[i]=i;
    assert(pk_checksum(input,32)==0x7f45);
    assert(pk_checksum(input,512)==0xecae);
    for(unsigned size=32;size<=512;size*=2)for(unsigned window=1;window<=2;++window)exchange(size,window);
    start(128,2);
    assert(pk_send(&a.p,input,128,now)==0);
    a.wire[30]^=1;deliver(&a,&b);assert(!b.p.rx_count);
    deliver(&b,&a);assert(a.used);pump();assert(b.p.rx_count==1);
    assert(pk_take(&b.p,out,128,&eof)==128 && !memcmp(out,input,128));pump();assert(!a.p.tx_count);
    /* Lost frame recovered by the retransmission timer. */
    assert(pk_send(&a.p,input,17,now)==0);a.used=0;now+=PK_RETRY;pk_tick(&a.p,now);pump();
    assert(pk_take(&b.p,out,512,&eof)==17);pump();
    /* An impossible ACK cannot release an outstanding frame. */
    assert(pk_send(&a.p,input,3,now)==0);a.used=0;
    control(&a,PK_RR<<3|7);assert(a.p.tx_count==1);
    /* A valid duplicate is not delivered twice. */
    now+=PK_RETRY;pk_tick(&a.p,now);uint8_t saved[518];size_t n=a.used;memcpy(saved,a.wire,n);
    pump();for(size_t i=0;i<n;++i)pk_receive(&b.p,saved[i],now);
    assert(b.p.rx_count==1);assert(pk_take(&b.p,out,512,&eof)==3);pump();
    /* Backpressure in the UART queue preserves a whole unsent frame. */
    a.blocked=true;assert(pk_send(&a.p,input,5,now)==0 && !a.p.tx[a.p.tx_head].sent);
    a.blocked=false;pk_tick(&a.p,++now);pump();assert(pk_take(&b.p,out,512,&eof)==5);pump();
    /* A checksum-valid malformed short prefix must not release its claimed ACK. */
    start(32,2);assert(!pk_send(&a.p,input,1,now));a.used=0;
    uint8_t body[32]={0};unsigned ctl=0300|010|1;
    uint16_t sum=NX_PK_CHECK-(pk_checksum(body,32)^ctl);
    uint8_t header[]={16,1,sum,sum>>8,ctl,1^(sum&255)^(sum>>8)^ctl};
    for(unsigned i=0;i<6;++i)pk_receive(&a.p,header[i],now);
    for(unsigned i=0;i<32;++i)pk_receive(&a.p,body[i],now);
    assert(a.p.tx_count==1 && !a.p.rx_count);
    /* Bounds, corrupt headers, truncated frames, resynchronization, and startup timeout. */
    start(32,2);uint8_t bad[]={16,255,0,0,0,255};
    for(unsigned i=0;i<6;++i)pk_receive(&a.p,bad[i],now);
    assert(!a.p.body_size);a.used=0;
    pk_receive(&a.p,16,now);pk_tick(&a.p,now+201);assert(!a.p.header_count);
    memset(&a,0,sizeof(a));pk_initialize(&a.p,32,2,emit,notice,&a,0);a.used=0;
    for(unsigned i=1;i<=PK_RETRIES+1;++i){pk_tick(&a.p,i*100);a.used=0;}
    assert(a.p.state==NX_PK_DOWN);
    start(512,2);a.blocked=true;uint32_t random=1;
    for(unsigned i=0;i<200000;++i){
        random=random*1664525U+1013904223U;pk_receive(&a.p,random>>24,i/100);
        assert(a.p.header_count<6 && a.p.body_count<=512 && a.p.body_size<=512 && a.p.rx_count<=2);
    }
    puts("PASS: V7 packet sizes/windows, sequence wrap, short/EOF, CRC, ACK ownership, retry, backpressure and cleanup");
}
