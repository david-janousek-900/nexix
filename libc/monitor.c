/* Native adaptation of V7 gen/mon.c, crt/mcount.s and csu/mcrt0.s. */
#include <mon.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
void *countbase;
static struct {
    unsigned char *buffer;
    size_t bytes,histogram;
    uintptr_t low,high;
    uint32_t capacity,used;
    int active;
} session;
static unsigned char busy;
static void put(unsigned char *p,uint64_t value,unsigned n)
{for(unsigned i=0;i<n;++i)p[i]=(unsigned char)(value>>(8*i));}
static uint64_t get(const unsigned char *p)
{uint64_t value=0;for(unsigned i=0;i<8;++i)value|=(uint64_t)p[i]<<(8*i);return value;}
static int fail(int error){errno=error;return -1;}
/* floor((numerator/denominator)*32768), without 128-bit division helpers.
 * Here numerator < denominator <= UINTPTR_MAX/2, so doubling cannot overflow. */
static unsigned fraction(size_t numerator,uintptr_t denominator)
{
    unsigned result=0;
    for(unsigned i=0;i<15;++i){
        numerator*=2;result<<=1;
        if(numerator>=denominator){numerator-=denominator;result|=1;}
    }
    return result;
}
static int start(void *lowpc,void *highpc,unsigned short *buffer,size_t words,size_t calls)
{
    uintptr_t low=(uintptr_t)lowpc,high=(uintptr_t)highpc,base=(uintptr_t)buffer;
    if(!base||base%2||high<=low||calls>UINT32_MAX||words>SIZE_MAX/2)return fail(EINVAL);
    size_t bytes=words*2;
    if(calls>(SIZE_MAX-NX_MON_HEADER_SIZE)/NX_MON_CALL_SIZE)return fail(EINVAL);
    size_t histogram=NX_MON_HEADER_SIZE+calls*NX_MON_CALL_SIZE;
    if(bytes<histogram||bytes>UINTPTR_MAX-base)return fail(EINVAL);
    if(profil(NULL,0,0,0))return -1;
    session.active=0;countbase=NULL;
    unsigned char *out=(unsigned char *)buffer;
    memset(out,0,histogram); /* Caller retains histogram contents, as V7 mon.c. */
    memcpy(out,NX_MON_MAGIC,8);
    size_t histwords=(bytes-histogram)/2;
    uintptr_t textwords=(high-low)>>1;
    unsigned scale=!histwords?0:histwords>=textwords?65534:fraction(histwords,textwords)*2;
    put(out+8,NX_MON_VERSION,4);put(out+12,NX_MON_MACHINE,2);put(out+14,64,2);
    put(out+16,low,8);put(out+24,high,8);put(out+32,NX_CLK_TCK,4);put(out+36,scale,4);
    put(out+40,calls,4);put(out+48,bytes-histogram,8);put(out+56,16,4);put(out+60,2,2);
    session.buffer=out;session.bytes=bytes;session.histogram=histogram;
    session.low=low;session.high=high;session.capacity=calls;session.used=0;
    if(profil((unsigned short *)(out+histogram),bytes-histogram,low,scale))return -1;
    session.active=1;countbase=calls?out+NX_MON_HEADER_SIZE:NULL;
    return 0;
}
static int stop(void)
{
    if(profil(NULL,0,0,0))return -1;
    session.active=0;countbase=NULL;
    if(!session.buffer)return 0;
    int fd=creat("mon.out",0666);if(fd<0)return -1;
    size_t done=0;int error=0;
    while(done<session.bytes){
        ssize_t n=write(fd,session.buffer+done,session.bytes-done);
        if(n<0){if(errno==EINTR)continue;error=errno;break;}
        if(!n||(size_t)n>session.bytes-done){error=EIO;break;}
        done+=(size_t)n;
    }
    if(close(fd)<0&&!error)error=errno;
    return error?fail(error):0;
}
int monitor(void *lowpc,...)
{
    if(__atomic_test_and_set(&busy,__ATOMIC_ACQUIRE))return fail(EBUSY);
    int result;
    if(!lowpc)result=stop();
    else{
        va_list args;va_start(args,lowpc);
        void *high=va_arg(args,void *);unsigned short *buffer=va_arg(args,unsigned short *);
        size_t words=va_arg(args,size_t),calls=va_arg(args,size_t);va_end(args);
        result=start(lowpc,high,buffer,words,calls);
    }
    __atomic_clear(&busy,__ATOMIC_RELEASE);return result;
}
/* mcount.S preserves all caller state. No allocation, syscalls or errno changes
 * here; a signal re-entering an update skips that nested call. */
void __nx_mcount(uintptr_t pc)
{
    if(__atomic_test_and_set(&busy,__ATOMIC_ACQUIRE))return;
    if(session.active&&pc>=session.low&&pc<session.high){
        unsigned char *records=session.buffer+NX_MON_HEADER_SIZE;
        uint32_t i;
        for(i=0;i<session.used;++i)if(get(records+(size_t)i*16)==pc)break;
        if(i<session.capacity&&(i<session.used||countbase)){
            unsigned char *record=records+(size_t)i*16;
            if(i==session.used){
                put(record,pc,8);++session.used;put(session.buffer+44,session.used,4);
                countbase=session.used<session.capacity?records+(size_t)session.used*16:NULL;
            }
            put(record+8,get(record+8)+1,8);
        }
    }
    __atomic_clear(&busy,__ATOMIC_RELEASE);
}
static void finish(void)
{
    (void)fflush(NULL); /* Registered first: user atexit callbacks run before us. */
    (void)monitor(NULL);
}
int __nx_profile_start(void *low,void *high)
{
    uintptr_t range=(uintptr_t)high-(uintptr_t)low;
    if((uintptr_t)high<=(uintptr_t)low)return fail(EINVAL);
    size_t histogram=range/8+(range%8!=0);
    size_t fixed=NX_MON_HEADER_SIZE+NX_MON_CALL_SIZE*NX_MON_DEFAULT_CALLS;
    if(histogram>(SIZE_MAX-fixed)/2)return fail(ENOMEM);
    size_t bytes=fixed+histogram*2;
    unsigned short *buffer=calloc(1,bytes);
    if(!buffer)return fail(ENOMEM);
    if(monitor(low,high,buffer,bytes/2,(size_t)NX_MON_DEFAULT_CALLS)){
        int error=errno;
        if(session.buffer==(unsigned char *)buffer){session.active=0;session.buffer=NULL;countbase=NULL;}
        free(buffer);return fail(error);
    }
    if(atexit(finish)){
        (void)profil(NULL,0,0,0);session.active=0;session.buffer=NULL;countbase=NULL;
        free(buffer);return fail(ENOMEM);
    }
    return 0;
}
