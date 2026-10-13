#include "storage.h"
#include <nexix/file.h>
#include <nexix/errno.h>
#include <string.h>
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
static unsigned char disk[1536],buffer[1536];
static int fail_read(struct block_device *d,uint32_t b,void *p){(void)d;(void)b;(void)p;return -NX_EIO;}
static int fail_write(struct block_device *d,uint32_t b,const void *p){(void)d;(void)p;return b==1?-NX_EIO:0;}
int main(void)
{
    struct ramdisk ram;struct storage_handle *h;size_t done;
    CHECK(ramdisk_init(&ram,disk,sizeof(disk),false)==0);
    CHECK(storage_bind(1,&ram.device,false)==0);
    CHECK(storage_open(0x0001,false,true,&h)==0);
    CHECK(storage_bind(1,NULL,false)==-NX_EBUSY);
    memset(buffer,0x5a,sizeof(buffer));
    CHECK(storage_transfer(h,511,buffer,515,true,&done)==0&&done==515);
    CHECK(disk[510]==0&&disk[511]==0x5a&&disk[1025]==0x5a&&disk[1026]==0);
    memset(buffer,0,sizeof(buffer));
    CHECK(storage_transfer(h,510,buffer,517,false,&done)==0&&done==517&&buffer[0]==0&&buffer[1]==0x5a&&buffer[516]==0);
    CHECK(storage_transfer(h,1535,buffer,2,true,&done)==-NX_EIO&&done==1);
    CHECK(storage_transfer(h,1536,buffer,1,false,&done)==0&&done==0);
    CHECK(storage_transfer(h,1537,buffer,1,false,&done)==-NX_EIO&&done==0);
    ram.device.read=fail_read;
    CHECK(storage_transfer(h,0,buffer,1,false,&done)==-NX_EIO&&done==0);
    ram.device.write=fail_write;
    CHECK(storage_transfer(h,0,buffer,1024,true,&done)==-NX_EIO&&done==512);
    ram.device.read_only=true;
    CHECK(storage_transfer(h,0,buffer,512,true,&done)==-NX_EROFS&&done==0);
    storage_close(h);CHECK(storage_bind(1,NULL,false)==0);
    CHECK(storage_open(1,false,false,&h)==-NX_ENODEV);
    CHECK(storage_open(0x0100,false,false,&h)==-NX_ENODEV);
    CHECK(storage_open(0x0408,true,false,&h)==-NX_ENODEV);
    CHECK(ramdisk_init(&ram,disk,sizeof(disk),false)==0&&storage_bind(1,&ram.device,true)==0);
    CHECK(storage_open(1,false,true,&h)==-NX_EBUSY);
    CHECK(storage_open(0x0401,true,false,&h)==0);
    CHECK(storage_transfer(h,0,buffer,512,true,&done)==-NX_EBUSY);
    storage_close(h);CHECK(storage_bind(1,NULL,false)==0);
    return 0;
}
