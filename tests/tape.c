#include "tape.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char image[200000],source[65534],dest[65534];
static size_t size;static unsigned failures,calls;static bool badwrite;
static uint64_t failat=UINT64_MAX;
static int rd(void *p,uint64_t at,void *b,size_t n)
{(void)p;++calls;if(failures){--failures;return -NX_EIO;}if(at+n>size)return -NX_EIO;memcpy(b,image+at,n);return 0;}
static int wr(void *p,uint64_t at,const void *b,size_t n)
{(void)p;++calls;if(badwrite || at==failat)return -NX_EIO;if(at+n>sizeof(image))return -NX_ENOSPC;memcpy(image+at,b,n);if(at+n>size)size=at+n;return 0;}
static int in(void *p,size_t at,void *b,size_t n){memcpy(b,(char *)p+at,n);return 0;}
static int out(void *p,size_t at,void *b,size_t n){memcpy((char *)p+at,b,n);return 0;}
static struct tape_media media(bool write){return (struct tape_media){NULL,size,rd,write?wr:NULL};}
static void fresh(struct tape *t,unsigned attempts,uint64_t cap)
{size=0;failures=0;badwrite=false;failat=UINT64_MAX;assert(!tape_image(t,media(true),attempts,true,cap));}
int main(void)
{
    struct tape t;for(unsigned i=0;i<sizeof(source);++i)source[i]=(unsigned char)i;
    fresh(&t,10,sizeof(image));assert(t.end==64 && !t.records);
    assert(tape_write_record(&t,2,in,source)==2);assert(!tape_mark(&t));
    assert(tape_write_record(&t,512,in,source)==512);assert(!tape_mark(&t));assert(!tape_mark(&t));
    assert(!tape_rewind(&t));assert(!tape_read_record(&t,0,out,dest) && !t.record);
    assert(tape_read_record(&t,512,out,dest)==2 && t.residual==510 && !memcmp(source,dest,2));
    assert(!tape_read_record(&t,512,out,dest) && t.record==2);
    assert(tape_read_record(&t,2,out,dest)==-NX_EIO && t.record==3 && !t.poisoned);
    assert(!tape_move(&t,true));assert(tape_read_record(&t,512,out,dest)==512 && !memcmp(source,dest,512));
    assert(!tape_rewind(&t));assert(tape_move(&t,true)==-NX_ENXIO);
    assert(tape_write_record(&t,4,in,source)==4 && t.records==1); /* overwrite discards suffix */
    assert(!tape_image(&t,media(true),10,false,0) && t.records==1);
    assert(tape_read_record(&t,512,out,dest)==4);assert(!tape_read_record(&t,512,out,dest));
    assert(!tape_rewind(&t));image[72]^=1;memset(dest,0xa5,sizeof(dest));
    assert(tape_read_record(&t,512,out,dest)==-NX_EIO && t.poisoned && t.record==0);
    for(unsigned i=0;i<sizeof(dest);++i)assert(dest[i]==0xa5);
    image[72]^=1;assert(!tape_image(&t,media(false),10,false,0));
    assert(tape_mark(&t)==-NX_EROFS && !t.poisoned);
    fresh(&t,2,100);assert(tape_write_record(&t,32,in,source)==-NX_ENOSPC && !t.records && t.end==64);
    assert(!tape_mark(&t) && !tape_mark(&t) && tape_mark(&t)==-NX_ENOSPC);
    fresh(&t,10,sizeof(image));assert(tape_write_record(&t,65534,in,source)==65534);
    assert(!tape_rewind(&t) && tape_read_record(&t,sizeof(dest),out,dest)==65534 && !memcmp(dest,source,sizeof(dest)));
    assert(tape_write_record(&t,65535,in,source)==-NX_EINVAL);
    fresh(&t,10,sizeof(image));assert(tape_write_record(&t,3,in,source)==3);assert(!tape_rewind(&t));
    assert(tape_read_record(&t,4,out,dest)==3 && !memcmp(dest,source,3));
    image[48]^=1;assert(tape_image(&t,media(true),2,false,0)==-NX_EIO);image[48]^=1;
    --size;assert(tape_image(&t,media(true),2,false,0)==-NX_EIO);++size;
    assert(!tape_image(&t,media(true),10,false,0));image[t.end-8]^=1;
    assert(tape_move(&t,false)==-NX_EIO && t.poisoned);image[t.end-8]^=1;
    for(unsigned attempts=2;attempts<=10;attempts+=8){
        failures=attempts-1;calls=0;assert(!tape_image(&t,media(true),attempts,false,0) && calls==attempts);
        failures=attempts;calls=0;assert(tape_image(&t,media(true),attempts,false,0)==-NX_EIO && calls==attempts);
    }
    fresh(&t,2,sizeof(image));badwrite=true;calls=0;assert(tape_mark(&t)==-NX_EIO && calls==2 && t.poisoned);
    badwrite=false;assert(!tape_image(&t,media(true),2,false,0) && !t.records);
    assert(tape_image(&t,media(true),2,true,sizeof(image))==-NX_EEXIST);
    fresh(&t,2,sizeof(image));assert(tape_write_record(&t,2,in,source)==2);
    failat=0;assert(tape_write_record(&t,4,in,source)==-NX_EIO && t.poisoned);
    failat=UINT64_MAX;assert(!tape_image(&t,media(true),2,false,0) && t.records==1);
    assert(tape_read_record(&t,512,out,dest)==2 && !memcmp(dest,source,2));
    assert(!tape_rewind(&t));failat=64;assert(tape_write_record(&t,4,in,source)==-NX_EIO && t.poisoned);
    failat=UINT64_MAX;assert(!tape_image(&t,media(true),2,false,0) && !t.records && t.end==64);
    puts("PASS: tape framing, records/marks, reverse spacing, overwrite, CRC atomicity, capacity, reopen, read-only and HT/TM retries");
}
