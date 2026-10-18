#include "dectape.h"
#include "storage.h"
#include <nexix/errno.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char image[NX_TC_BYTES],bytes[512],copy[512];
static struct block_device base;static unsigned calls,failures;static int fault;
static int rd(struct block_device *d,uint32_t b,void *p)
{(void)d;++calls;if(failures){--failures;memset(p,0xee,512);return fault;}memcpy(p,image+b*512,512);return 0;}
static int wr(struct block_device *d,uint32_t b,const void *p)
{(void)d;++calls;if(failures){--failures;return fault;}memcpy(image+b*512,p,512);return 0;}
int main(void)
{
    base=(struct block_device){.blocks=578,.read=rd,.write=wr};struct storage_handle *h,*alias;size_t n;
    assert(!storage_raw_device(NX_TC_DEV(0)) && storage_block_device(NX_TC_DEV(0)));
    assert(tc_bind(8,&base)==-NX_ENXIO);base.blocks=577;assert(tc_bind(0,&base)==-NX_EINVAL);base.blocks=578;
    for(unsigned unit=0;unit<8;++unit){
        assert(!tc_bind(unit,&base) && !storage_open(NX_TC_DEV(unit),false,true,&h));
        memset(bytes,unit+1,512);assert(!storage_transfer(h,577*512,bytes,512,true,&n) && n==512);
        assert(!storage_transfer(h,577*512,copy,512,false,&n) && n==512 && !memcmp(copy,bytes,512));
        assert(storage_transfer(h,NX_TC_BYTES,copy,1,false,&n)==-NX_EIO && !n);
        assert(tc_bind(unit,NULL)==-NX_EBUSY);storage_close(h);assert(!tc_bind(unit,NULL));
    }
    assert(storage_open(NX_TC_DEV(8),false,false,&h)==-NX_ENXIO);
    assert(storage_open(NX_TC_DEV(0),true,false,&h)==-NX_ENODEV);
    assert(!tc_bind(0,&base) && !tc_bind(7,&base));
    assert(!storage_open(NX_TC_DEV(0),false,true,&h));
    assert(!storage_transfer(h,513,"random",6,true,&n) && n==6);
    assert(!storage_transfer(h,513,copy,6,false,&n) && n==6 && !memcmp(copy,"random",6));
    fault=-NX_EIO;failures=19;calls=0;assert(!storage_transfer(h,512,copy,512,false,&n) && calls==20);
    failures=20;calls=0;memset(copy,0xa5,512);assert(storage_transfer(h,0,copy,512,false,&n)==-NX_EIO && calls==20 && !n);
    for(unsigned i=0;i<512;++i)assert(copy[i]==0xa5);
    assert(!storage_transfer(h,0,copy,512,false,&n)); /* exhaustion is not tcper */
    assert(!storage_open(NX_TC_DEV(0),false,false,&alias));fault=-NX_ENXIO;failures=1;calls=0;
    assert(storage_transfer(h,0,copy,512,false,&n)==-NX_ENXIO && calls==1);
    assert(storage_transfer(alias,0,copy,512,false,&n)==-NX_EIO && calls==1);
    struct storage_handle *other;assert(!storage_open(NX_TC_DEV(7),false,false,&other));
    assert(!storage_transfer(other,0,copy,512,false,&n));storage_close(other);
    storage_close(h);assert(storage_transfer(alias,0,copy,512,false,&n)==-NX_EIO);storage_close(alias);
    assert(!storage_open(NX_TC_DEV(0),false,false,&h) && !storage_transfer(h,0,copy,512,false,&n));storage_close(h);
    assert(!storage_open(NX_TC_DEV(0),false,true,&h));base.read_only=true;
    assert(storage_transfer(h,0,bytes,512,true,&n)==-NX_EROFS && !n);base.read_only=false;storage_close(h);
    assert(!storage_mount_open(NX_TC_DEV(0),false,&h));
    assert(storage_open(NX_TC_DEV(7),false,true,&alias)==-NX_EBUSY);
    assert(storage_mount_open(NX_TC_DEV(7),false,&alias)==-NX_EBUSY);
    storage_mount_close(h);base.read_only=true;
    assert(storage_open(NX_TC_DEV(0),false,true,&h)==-NX_EROFS);
    assert(!storage_open(NX_TC_DEV(0),false,false,&h));base.read_only=false;storage_close(h);
    assert(!tc_bind(0,NULL) && !tc_bind(7,NULL) && !tc_bound(&base));
    puts("PASS: eight TC units, random/partial/last-block I/O, bounds, 20 attempts, atomic failed reads, sticky select errors, final close, read-only and mounted aliases");
}
