#include "storage.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct block_device media[8];
static unsigned calls,failures;static int fail_error=-NX_EIO;
static uint32_t last;static unsigned which;
static int read_one(struct block_device *d,uint32_t b,void *p)
{
    ++calls;last=b;which=d-media;memset(p,(b+which)&255,512);
    if(failures){--failures;return fail_error;}return 0;
}
static int write_one(struct block_device *d,uint32_t b,const void *p)
{(void)p;++calls;last=b;which=d-media;if(failures){--failures;return fail_error;}return 0;}
static void opened(unsigned kind,unsigned minor,uint32_t first,uint32_t blocks,unsigned unit)
{
    struct storage_handle *h;uint8_t bytes[512];size_t n;
    uint16_t dev=NX_DISK_DEV(kind,minor,false);assert(!storage_open(dev,false,true,&h));
    assert(h->device->blocks==blocks && disk_bind(kind,unit,NULL)==-NX_EBUSY);
    assert(!storage_transfer(h,0,bytes,512,false,&n) && n==512 && last==first && which==unit);
    assert(!storage_transfer(h,(uint64_t)(blocks-1)*512,bytes,512,true,&n) && n==512 && last==first+blocks-1);
    assert(!storage_transfer(h,(uint64_t)blocks*512,bytes,1,false,&n) && !n);
    assert(storage_transfer(h,(uint64_t)blocks*512,bytes,1,true,&n)==-NX_EIO && !n);
    storage_close(h);
    dev=NX_DISK_DEV(kind,minor,true);assert(!storage_open(dev,true,false,&h));storage_close(h);
}
int main(void)
{
    static const unsigned units[]={2,1,4,4,8};
    for(unsigned u=0;u<8;++u)media[u]=(struct block_device){.blocks=340670,.read=read_one,.write=write_one};
    for(unsigned k=0;k<5;++k)for(unsigned u=0;u<units[k];++u)assert(!disk_bind(k,u,&media[u]));
    uint32_t hp_n[]={9614,8778,0,0,161348,160930,153406,322278},hp_a[]={0,9614,0,0,18392,179740,18392,18392};
    uint32_t rp_n[]={81000,5000,2000,74000,0,0,0,0},rp_a[]={0,0,5000,7000,0,0,0,0};
    struct storage_handle *h,*mounted,*other;size_t n;uint8_t bytes[1024];
    for(unsigned k=0;k<5;++k)for(unsigned u=0;u<units[k];++u){
        if(k==NX_DISK_HP || k==NX_DISK_RP){for(unsigned p=0;p<8;++p){
            uint32_t count=k==NX_DISK_HP?hp_n[p]:rp_n[p],first=k==NX_DISK_HP?hp_a[p]:rp_a[p];
            if(count)opened(k,u*8+p,first,count,u);
            else assert(storage_open(NX_DISK_DEV(k,u*8+p,false),false,false,&h)==-NX_ENXIO);
        }}else if(k==NX_DISK_RF)for(unsigned p=0;p<8;++p)opened(k,p,0,1024*(p+1),0);
        else opened(k,u,0,k==NX_DISK_RK?4872:10240,u);
        assert(storage_open(NX_DISK_DEV(k,255,false),false,false,&h)==-NX_ENXIO);
    }
    unsigned dev=NX_DISK_DEV(NX_DISK_RK,0,false);
    assert(!storage_open(dev,false,true,&h));calls=0;failures=2;
    assert(!storage_transfer(h,0,bytes,512,false,&n) && calls==3 && n==512);
    memset(bytes,0xa5,512);calls=0;failures=100;
    assert(storage_transfer(h,0,bytes,512,false,&n)==-NX_EIO && calls==11 && !n && bytes[0]==0xa5);
    failures=1;fail_error=-NX_EROFS;calls=0;
    assert(storage_transfer(h,0,bytes,512,true,&n)==-NX_EROFS && calls==1 && !n);
    failures=0;fail_error=-NX_EIO;storage_close(h);
    assert(!storage_open(NX_DISK_DEV(NX_DISK_HP,0,false),false,false,&h));calls=0;failures=100;
    assert(storage_transfer(h,0,bytes,512,false,&n)==-NX_EIO && calls==29);failures=0;storage_close(h);
    /* Mount guards cover overlapping partitions, other families and legacy/raw aliases. */
    assert(!storage_bind(1,&media[0],false));
    assert(!storage_open(NX_DISK_DEV(NX_DISK_RP,0,true),true,true,&other));
    assert(!storage_mount_open(NX_DISK_DEV(NX_DISK_RP,1,false),false,&mounted));
    assert(storage_transfer(other,0,bytes,1,true,&n)==-NX_EBUSY);
    assert(storage_open(0x401,true,true,&h)==-NX_EBUSY);
    assert(storage_open(dev,false,true,&h)==-NX_EBUSY);
    assert(storage_mount_open(NX_DISK_DEV(NX_DISK_RF,0,false),true,&h)==-NX_EBUSY);
    assert(!storage_mount_open(NX_DISK_DEV(NX_DISK_RP,2,false),false,&h)); /* Adjacent, independent partition. */
    storage_mount_close(h);storage_mount_close(mounted);storage_close(other);
    assert(storage_bind(1,NULL,false)==-NX_EBUSY); /* Binding still owns backing lifetime. */
    media[0].read_only=true;assert(storage_open(dev,false,true,&h)==-NX_EROFS);
    assert(!storage_open(dev,false,false,&h));media[0].read_only=false;
    assert(!storage_open(dev,false,true,&other));storage_close(other);storage_close(h);
    unsigned capacity=media[0].blocks;media[0].blocks=1;
    assert(storage_open(dev,false,false,&h)==-NX_ENXIO);media[0].blocks=capacity;
    /* Exhaust all active view slots, then release and reuse every one. */
    struct storage_handle *held[32];unsigned used=0;
    for(unsigned k=0;k<5 && used<32;++k)for(unsigned m=0;m<64 && used<32;++m)
        if(!storage_open(NX_DISK_DEV(k,m,false),false,false,&h))held[used++]=h;
    assert(used==32 && storage_open(NX_DISK_DEV(NX_DISK_RP,63,false),false,false,&h)==-NX_ENFILE);
    while(used)storage_close(held[--used]);
    for(unsigned k=0;k<5;++k)for(unsigned u=0;u<units[k];++u)assert(!disk_bind(k,u,NULL));
    assert(!storage_bind(1,NULL,false));
    assert(storage_open(dev,false,false,&h)==-NX_ENODEV);
    puts("PASS: five V7 disk families, every partition/unit, last sectors, retries/errors, mount alias guards and view lifetime/exhaustion");
}
