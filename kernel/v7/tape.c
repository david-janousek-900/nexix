/* Sequential records and tape marks replacing V7 ht/tm register operations. */
#include "tape.h"
#include <nexix/errno.h>
#define HEADER 64U
static const uint8_t magic[8]={'N','X','T','A','P','E','1',0};
static void clear(void *p,size_t n){uint8_t *b=p;while(n--)*b++=0;}
static uint32_t get32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t get64(const uint8_t *p){return get32(p)|((uint64_t)get32(p+4)<<32);}
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=n>>(i*8);}
static void put64(uint8_t *p,uint64_t n){put32(p,n);put32(p+4,n>>32);}
static uint32_t hash(uint32_t h,const uint8_t *p,size_t n){while(n--)h=(h^*p++)*16777619U;return h;}
static uint32_t header_hash(const uint8_t *p){return hash(hash(2166136261U,p,48),p+52,12);}
static int media_read(struct tape *t,uint64_t at,void *p,size_t n)
{
    if(at>t->media.size || n>t->media.size-at){t->poisoned=true;return -NX_EIO;}
    int error=-NX_EIO;
    for(unsigned i=0;i<t->attempts;++i){error=t->media.read(t->media.context,at,p,n);if(error!=-NX_EIO)break;}
    if(error)t->poisoned=true;
    return error;
}
static int media_write(struct tape *t,uint64_t at,const void *p,size_t n)
{
    if(!t->media.write)return -NX_EROFS;
    int error=-NX_EIO;
    for(unsigned i=0;i<t->attempts;++i){error=t->media.write(t->media.context,at,p,n);if(error!=-NX_EIO)break;}
    if(error)t->poisoned=true;
    else if(at+n>t->media.size)t->media.size=at+n;
    return error;
}
static int commit(struct tape *t,uint64_t end,uint64_t records)
{
    uint8_t h[HEADER]={0};for(unsigned i=0;i<8;++i)h[i]=magic[i];
    put32(h+8,1);put32(h+12,HEADER);put64(h+16,end);put64(h+24,records);
    put64(h+32,t->capacity);put64(h+40,t->generation+1);put32(h+48,header_hash(h));
    int error=media_write(t,0,h,sizeof(h));if(error)return error;
    t->end=end;t->records=records;++t->generation;return 0;
}
int tape_image(struct tape *t,struct tape_media media,unsigned attempts,bool initialize,uint64_t capacity)
{
    clear(t,sizeof(*t));t->media=media;t->attempts=attempts;
    if(!media.read || !attempts || attempts>10)return -NX_EINVAL;
    if(initialize){
        if(media.size)return -NX_EEXIST;
        if(capacity<HEADER+32 || capacity>NX_TAPE_MAX_IMAGE)return -NX_EINVAL;
        t->capacity=capacity;int error=commit(t,HEADER,0);if(error)return error;
    }else{
        uint8_t h[HEADER];int error=media_read(t,0,h,sizeof(h));if(error)return error;
        for(unsigned i=0;i<8;++i)if(h[i]!=magic[i])return -NX_EIO;
        for(unsigned i=52;i<HEADER;++i)if(h[i])return -NX_EIO;
        if(get32(h+8)!=1 || get32(h+12)!=HEADER || get32(h+48)!=header_hash(h))return -NX_EIO;
        t->end=get64(h+16);t->records=get64(h+24);t->capacity=get64(h+32);t->generation=get64(h+40);
        if(t->capacity<HEADER+32 || t->capacity>NX_TAPE_MAX_IMAGE || t->end<HEADER ||
           t->end>media.size || t->end>t->capacity || t->records>NX_TAPE_MAX_RECORDS ||
           t->records>(t->end-HEADER)/16 || (!t->records && t->end!=HEADER))return -NX_EIO;
    }
    t->position=HEADER;return 0;
}
static int frame(struct tape *t,uint64_t at,uint32_t *length,uint32_t *sum,uint64_t *next)
{
    uint8_t h[8],tail[8];
    if(at<HEADER || at>t->end || t->end-at<16)return -NX_EIO;
    int error=media_read(t,at,h,8);if(error)return error;
    *length=get32(h);*sum=get32(h+4);
    if(*length>NX_TAPE_MAX_RECORD || (!*length && *sum))return -NX_EIO;
    uint64_t size=16+((*length+1U)&~1U);
    if(size>t->end-at)return -NX_EIO;
    *next=at+size;error=media_read(t,*next-8,tail,8);if(error)return error;
    for(unsigned i=0;i<8;++i)if(h[i]!=tail[i])return -NX_EIO;
    return 0;
}
static int corrupt(struct tape *t,int error){if(error)t->poisoned=true;return error;}
int tape_length(struct tape *t,uint32_t *length)
{
    if(t->poisoned)return -NX_EIO;
    if(t->position==t->end)return -NX_ENXIO;
    uint32_t sum;uint64_t next;return corrupt(t,frame(t,t->position,length,&sum,&next));
}
int tape_move(struct tape *t,bool backward)
{
    if(t->poisoned)return -NX_EIO;
    if(backward){
        if(t->position==HEADER)return -NX_ENXIO;
        uint8_t footer[8];int error=media_read(t,t->position-8,footer,8);if(error)return error;
        uint32_t n=get32(footer);uint64_t size=16+((n+1ULL)&~1ULL);
        if(n>NX_TAPE_MAX_RECORD || size>t->position-HEADER || !t->record)return corrupt(t,-NX_EIO);
        uint32_t length,sum;uint64_t next;
        error=frame(t,t->position-size,&length,&sum,&next);
        if(error || next!=t->position)return corrupt(t,error?error:-NX_EIO);
        t->position-=size;--t->record;return 0;
    }
    if(t->position==t->end)return -NX_ENXIO;
    uint32_t length,sum;uint64_t next;int error=frame(t,t->position,&length,&sum,&next);
    if(error || t->record>=t->records)return corrupt(t,error?error:-NX_EIO);
    t->position=next;++t->record;return 0;
}
int tape_rewind(struct tape *t)
{if(t->poisoned)return -NX_EIO;t->position=HEADER;t->record=0;return 0;}
int64_t tape_read_record(struct tape *t,size_t count,tape_copy sink,void *context)
{
    t->residual=count;if(t->poisoned)return -NX_EIO;
    if(!count)return 0;
    if(t->position==t->end){if(t->record!=t->records)return corrupt(t,-NX_EIO);return 0;}
    uint32_t length,sum;uint64_t next;int error=frame(t,t->position,&length,&sum,&next);
    if(error || t->record>=t->records)return corrupt(t,error?error:-NX_EIO);
    if(length>count){t->position=next;++t->record;return -NX_EIO;} /* Record consumed, as on physical tape. */
    uint8_t bytes[512];uint32_t h=2166136261U;
    for(unsigned pass=0;pass<2;++pass)for(unsigned off=0;off<length;){
        unsigned n=length-off;if(n>sizeof(bytes))n=sizeof(bytes);
        error=media_read(t,t->position+8+off,bytes,n);if(error)return error;
        if(!pass)h=hash(h,bytes,n);else if((error=sink(context,off,bytes,n)))return error;
        off+=n;
        if(!pass && off==length && h!=sum)return corrupt(t,-NX_EIO);
    }
    t->position=next;++t->record;t->residual=count-length;return length;
}
int tape_erase(struct tape *t)
{
    if(t->poisoned)return -NX_EIO;
    return commit(t,t->position,t->record);
}
int64_t tape_write_record(struct tape *t,size_t count,tape_copy source,void *context)
{
    t->residual=count;if(t->poisoned)return -NX_EIO;
    if(count>NX_TAPE_MAX_RECORD)return -NX_EINVAL;
    uint64_t size=16+((count+1ULL)&~1ULL);
    if(!t->media.write)return -NX_EROFS;
    if(t->record>=NX_TAPE_MAX_RECORDS || t->position>t->capacity || size>t->capacity-t->position)return -NX_ENOSPC;
    /* Publish a valid prefix before overwriting a suffix. Appended staged bytes
     * stay invisible until the final checked header is published. */
    int error=0;if(t->position<t->end)error=tape_erase(t);if(error)return error;
    uint8_t bytes[512],h[8];uint32_t sum=2166136261U;
    for(size_t off=0;off<count;){
        size_t n=count-off;if(n>sizeof(bytes))n=sizeof(bytes);
        error=source(context,off,bytes,n);if(error)return corrupt(t,error);
        sum=hash(sum,bytes,n);error=media_write(t,t->position+8+off,bytes,n);if(error)return error;off+=n;
    }
    if(count&1){uint8_t zero=0;error=media_write(t,t->position+8+count,&zero,1);if(error)return error;}
    put32(h,count);put32(h+4,count?sum:0);
    error=media_write(t,t->position,h,8);if(error)return error;
    error=media_write(t,t->position+size-8,h,8);if(error)return error;
    error=commit(t,t->position+size,t->record+1);if(error)return error;
    t->position=t->end;t->record=t->records;t->residual=0;return count;
}
int tape_mark(struct tape *t){int64_t n=tape_write_record(t,0,NULL,NULL);return n<0?(int)n:0;}
