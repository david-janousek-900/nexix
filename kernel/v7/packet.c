/* V7 dev/pk0.c and pk2.c: bounded native state machine and wire encoders. */
#include "packet.h"
#include <nexix/errno.h>
#define DATA 0200
#define SHORT 0100
static void clear(void *v,size_t n){uint8_t *p=v;while(n--)*p++=0;}
static unsigned code(unsigned size){unsigned k=1;while(size>32){size>>=1;++k;}return k;}
bool pk_size_valid(unsigned n){return n>=32 && n<=NX_PK_MAX_SIZE && !(n&(n-1));}
uint16_t pk_checksum(const uint8_t *s,unsigned n)
{
    uint16_t sum=0xffff,x=0;
    if(!n)return sum;
    do{
        sum=(uint16_t)((sum<<1)|(sum>>15));uint16_t old=sum;
        sum=(uint16_t)(sum+*s++);x=(uint16_t)(x+(sum^n));
        if(sum<=old)sum^=x;
    }while(--n);
    return sum;
}
static void notice(struct packet *p){if(p->notice)p->notice(p->context);}
static bool frame(struct packet *p,unsigned control,const struct pk_buffer *b)
{
    uint8_t wire[6+NX_PK_MAX_SIZE];wire[0]=NX_PK_SYN;wire[1]=b?code(p->xsize):9;
    uint16_t sum=NX_PK_CHECK-(b?(pk_checksum(b->data,p->xsize)^control):control);
    wire[2]=sum;wire[3]=sum>>8;wire[4]=control;wire[5]=wire[1]^wire[2]^wire[3]^wire[4];
    if(b)for(unsigned i=0;i<p->xsize;++i)wire[6+i]=b->data[i];
    return p->emit(p->context,wire,b?6+p->xsize:6);
}
static void output(struct packet *p)
{
    for(unsigned i=1;i<8;++i)if(p->pending&(1U<<i)){
        unsigned value=(i==PK_RJ || i==PK_RR)?p->rx_ack:
            (i==PK_INITA || i==PK_INITC)?p->rwindow:i==PK_INITB?code(p->rsize)-1:0;
        if(!frame(p,(i<<3)|value,NULL))return;
        p->pending&=~(1U<<i);
    }
    if(p->state!=NX_PK_LIVE)return;
    for(unsigned i=0;i<p->tx_count;++i){
        struct pk_buffer *b=&p->tx[(p->tx_head+i)%NX_PK_WINDOW];
        if(!b->sent){
            unsigned ctl=DATA|(b->sequence<<3)|p->rx_ack;
            if(b->length<p->xsize)ctl|=SHORT;
            if(!frame(p,ctl,b))return;
            b->sent=true;
        }
    }
}
static void reject(struct packet *p)
{
    if(!p->bad && p->state==NX_PK_LIVE)p->pending|=1U<<PK_RJ;
    p->bad=true;output(p);
}
void pk_initialize(struct packet *p,unsigned size,unsigned window,bool (*emit)(void *,const uint8_t *,size_t),void (*notify)(void *),void *context,uint64_t now)
{
    clear(p,sizeof(*p));p->rsize=size;p->rwindow=window;p->swindow=1;p->xsize=64;
    p->emit=emit;p->notice=notify;p->context=context;p->deadline=now+100;
    p->pending=1U<<PK_INITA;output(p);
}
static bool acknowledge(struct packet *p,unsigned ack,uint64_t now)
{
    if(ack==p->tx_ack)return true;
    unsigned n=0;
    for(;n<p->tx_count;++n){
        struct pk_buffer *b=&p->tx[(p->tx_head+n)%NX_PK_WINDOW];
        if(!b->sent)return false;
        if(b->sequence==ack)break;
    }
    if(n==p->tx_count)return false;
    ++n;p->tx_head=(p->tx_head+n)%NX_PK_WINDOW;p->tx_count-=n;p->tx_ack=ack;
    p->retries=0;p->deadline=now+PK_RETRY;notice(p);return true;
}
static void control(struct packet *p,unsigned c,uint64_t now)
{
    unsigned kind=c>>3,value=c&7;
    if((c&0300) || !kind){reject(p);return;}
    if(kind==PK_CLOSE){
        if(!p->remote_closed){p->pending|=1U<<PK_CLOSE;p->remote_closed=true;}
        p->state=NX_PK_DOWN;p->tx_count=0;output(p);notice(p);return;
    }
    if(p->state==NX_PK_DOWN)return;
    if(kind==PK_INITA){
        if(!value){reject(p);return;}
        p->swindow=value>NX_PK_WINDOW?NX_PK_WINDOW:value;p->init_a=true;p->pending|=1U<<PK_INITB;
        if(p->state==NX_PK_LIVE)p->pending|=1U<<PK_INITC;
    }else if(kind==PK_INITB){
        unsigned size=32U<<value;
        if(!pk_size_valid(size) || (p->state==NX_PK_LIVE && size!=p->xsize)){reject(p);return;}
        p->xsize=size;p->init_b=true;p->pending|=1U<<PK_INITC;
    }else if(kind==PK_INITC){
        if(value)p->swindow=value>NX_PK_WINDOW?NX_PK_WINDOW:value;
        if(p->init_a && p->init_b){p->state=NX_PK_LIVE;p->retries=0;p->deadline=now+PK_RETRY;notice(p);}
        else p->pending|=1U<<PK_INITB;
    }else if(kind==PK_RR || kind==PK_RJ){
        if(p->state!=NX_PK_LIVE || !acknowledge(p,value,now)){reject(p);return;}
        if(kind!=PK_RR){
            for(unsigned i=0;i<p->tx_count;++i)p->tx[(p->tx_head+i)%NX_PK_WINDOW].sent=false;
        }
    }
    p->bad=false;output(p);
}
static void data(struct packet *p,uint64_t now)
{
    unsigned ctl=p->header[4],seq=(ctl>>3)&7;
    uint16_t sum=p->header[2]|((uint16_t)p->header[3]<<8);
    if(!(ctl&DATA) || (uint16_t)((pk_checksum(p->body,p->rsize)^ctl)+sum)!=NX_PK_CHECK){reject(p);return;}
    if(p->state!=NX_PK_LIVE)return;
    unsigned length=p->rsize,prefix=0;
    if(ctl&SHORT){
        unsigned missing=p->body[0]&127;prefix=1;
        if(p->body[0]&128){missing|=(unsigned)p->body[1]<<7;prefix=2;}
        if(missing<prefix || missing>p->rsize){reject(p);return;}
        length=p->rsize-missing;
    }
    /* Validate the entire frame before releasing any acknowledged buffer. */
    (void)acknowledge(p,ctl&7,now);
    if(seq!=((p->rx_accept+1)&7) || p->rx_count==p->rwindow){
        bool duplicate=seq==p->rx_ack;
        for(unsigned i=0;i<p->rx_count;++i)duplicate|=p->rx[(p->rx_head+i)%NX_PK_WINDOW].sequence==seq;
        p->pending|=1U<<(duplicate || p->rx_count==p->rwindow?PK_RR:PK_RJ);output(p);return;
    }
    struct pk_buffer *b=&p->rx[(p->rx_head+p->rx_count)%NX_PK_WINDOW];
    b->length=length;b->offset=0;b->sequence=seq;
    for(unsigned i=0;i<length;++i)b->data[i]=p->body[prefix+i];
    ++p->rx_count;p->rx_accept=seq;p->bad=false;notice(p);output(p);
}
void pk_receive(struct packet *p,uint8_t byte,uint64_t now)
{
    p->last_byte=now;
    if(p->body_size){
        p->body[p->body_count++]=byte;
        if(p->body_count==p->body_size){p->body_size=p->body_count=0;data(p,now);p->header_count=0;}
        return;
    }
    if(!p->header_count && byte!=NX_PK_SYN)return;
    p->header[p->header_count++]=byte;if(p->header_count<6)return;
    unsigned k=p->header[1],c=p->header[4];
    if((p->header[1]^p->header[2]^p->header[3]^p->header[4])!=p->header[5] ||
       !k || (k!=9 && (k>5 || (32U<<(k-1))!=p->rsize))){
        reject(p);
        unsigned start=1;while(start<6 && p->header[start]!=NX_PK_SYN)++start;
        p->header_count=6-start;for(unsigned i=0;i<p->header_count;++i)p->header[i]=p->header[start+i];
        return;
    }
    if(k==9){
        uint16_t sum=p->header[2]|((uint16_t)p->header[3]<<8);p->header_count=0;
        if((uint16_t)(sum+c)!=NX_PK_CHECK){reject(p);return;}
        control(p,c,now);
    }else {p->body_size=p->rsize;p->body_count=0;}
}
bool pk_readable(const struct packet *p){return p->rx_count || (p->state==NX_PK_DOWN && !p->eof_reported);}
size_t pk_take(struct packet *p,uint8_t *bytes,size_t count,bool *eof)
{
    size_t done=0;*eof=false;
    while(count && p->rx_count){
        struct pk_buffer *b=&p->rx[p->rx_head];unsigned left=b->length-b->offset;
        if(!left && done)break;
        unsigned n=left<count?left:count;for(unsigned i=0;i<n;++i)bytes[done+i]=b->data[b->offset+i];
        done+=n;count-=n;b->offset+=n;
        if(b->offset==b->length){
            p->rx_ack=b->sequence;p->rx_head=(p->rx_head+1)%NX_PK_WINDOW;--p->rx_count;
            p->pending|=1U<<PK_RR;output(p);notice(p);
            if(!n){*eof=true;break;}
        }
    }
    if(!done && !p->rx_count && p->state==NX_PK_DOWN){*eof=true;p->eof_reported=true;}
    return done;
}
int pk_send(struct packet *p,const uint8_t *bytes,size_t n,uint64_t now)
{
    if(p->state!=NX_PK_LIVE)return -NX_EIO;
    if(n>p->xsize)return -NX_EINVAL;
    if(p->tx_count>=p->swindow)return -NX_EAGAIN;
    struct pk_buffer *b=&p->tx[(p->tx_head+p->tx_count)%NX_PK_WINDOW];clear(b,sizeof(*b));
    b->length=n;b->sequence=p->tx_sequence=(p->tx_sequence+1)&7;
    unsigned prefix=0;
    if(n<p->xsize){unsigned missing=p->xsize-n;prefix=1;b->data[0]=missing&127;
        if(missing>127){b->data[0]|=128;b->data[1]=missing>>7;prefix=2;}}
    for(unsigned i=0;i<n;++i)b->data[prefix+i]=bytes[i];
    if(!p->tx_count)p->deadline=now+PK_RETRY;
    ++p->tx_count;output(p);return 0;
}
void pk_shutdown(struct packet *p)
{p->state=NX_PK_DOWN;p->pending=1U<<PK_CLOSE;output(p);notice(p);}
void pk_tick(struct packet *p,uint64_t now)
{
    if((p->header_count || p->body_size) && now-p->last_byte>=200){p->header_count=p->body_size=p->body_count=0;reject(p);}
    if(p->state!=NX_PK_DOWN && now>=p->deadline){
        if(++p->retries>PK_RETRIES){pk_shutdown(p);return;}
        if(p->state!=NX_PK_LIVE){
            p->pending|=1U<<PK_INITA;
            if(p->init_a)p->pending|=1U<<PK_INITB;
            if(p->init_b)p->pending|=1U<<PK_INITC;
            p->deadline=now+100;
        }else{
            if(p->tx_count)for(unsigned i=0;i<p->tx_count;++i)p->tx[(p->tx_head+i)%NX_PK_WINDOW].sent=false;
            if(p->bad)p->pending|=1U<<PK_RJ;
            p->deadline=now+PK_RETRY;
            if(!p->tx_count && !p->bad)p->retries=0;
        }
    }
    output(p);
}
