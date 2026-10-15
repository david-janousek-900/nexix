#include "v7/fs.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char bytes[256*BLOCK_SIZE],original[256*BLOCK_SIZE];
static struct ramdisk ram;
static struct v7fs fs;
static int fault;
static int (*write_backing)(struct block_device *,uint32_t,const void *);
int64_t clock_now(void){return 1709210096;}
static int write_fault(struct block_device *d,uint32_t block,const void *data)
{if((int)block==fault)return -NX_EIO;return write_backing(d,block,data);}
static void reset(void)
{
    if(fs.mounted)assert(v7fs_unmount(&fs)==0);
    memcpy(bytes,original,sizeof(bytes));assert(ramdisk_init(&ram,bytes,sizeof(bytes),false)==0);
    write_backing=ram.device.write;ram.device.write=write_fault;fault=-1;
    assert(v7fs_mount(&fs,&ram.device,false)==0);
}
static v7_ino_t create(const char *name)
{
    char shortname[14]={0};memcpy(shortname,name,strlen(name));v7_ino_t ino;
    assert(v7fs_create(&fs,2,shortname,0600,0,0,&ino)==0);return ino;
}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(fread(original,1,sizeof(original),f)==sizeof(original));fclose(f);reset();
    v7_ino_t log=create("log"),fill=create("fill");
    unsigned char block[512],record[40],readback[540];memset(block,'X',sizeof(block));memset(record,0xa5,sizeof(record));size_t done;
    assert(v7fs_write(&fs,log,0,block,500,&done)==0&&done==500);
    uint64_t offset=0;int error;
    while(!(error=v7fs_write(&fs,fill,offset,block,512,&done))){assert(done==512);offset+=done;}
    assert(error==-NX_ENOSPC);
    assert(v7fs_append_record(&fs,log,record,40)==-NX_ENOSPC&&!fs.failed);
    struct v7fs_inode disk;assert(v7fs_inode_get(&fs,log,&disk)==0&&disk.size==500);
    assert(v7fs_read(&fs,log,0,readback,sizeof(readback),&done)==0&&done==500);
    for(unsigned i=0;i<500;++i)assert(readback[i]=='X');
    assert(v7fs_truncate(&fs,fill)==0&&v7fs_append_record(&fs,log,record,40)==0);
    assert(v7fs_read(&fs,log,0,readback,sizeof(readback),&done)==0&&done==540&&!memcmp(readback+500,record,40));
    assert(v7fs_unmount(&fs)==0&&v7fs_mount(&fs,&ram.device,false)==0);
    assert(v7fs_inode_get(&fs,log,&disk)==0&&disk.size==540);
    reset();log=create("log");assert(v7fs_write(&fs,log,0,block,500,&done)==0);
    assert(v7fs_inode_get(&fs,log,&disk)==0);
    fault=disk.addr[0];assert(v7fs_append_record(&fs,log,record,40)==-NX_EIO&&fs.failed);
    fault=-1;fs.failed=false;assert(v7fs_inode_get(&fs,log,&disk)==0&&disk.size==500);
    assert(v7fs_append_record(&fs,log,record,40)==0);
    /* Final inode-sector failure: data writes succeed, old EOF remains published. */
    fault=v7_itod(log);assert(v7fs_append_record(&fs,log,record,40)==-NX_EIO&&fs.failed);
    fault=-1;fs.failed=false;assert(v7fs_inode_get(&fs,log,&disk)==0&&disk.size==540);
    assert(v7fs_append_record(&fs,log,record,0)==-NX_EINVAL&&v7fs_append_record(&fs,log,block,513)==-NX_EINVAL);
    fs.read_only=true;assert(v7fs_append_record(&fs,log,record,40)==-NX_EROFS);fs.read_only=false;
    assert(v7fs_unmount(&fs)==0);
    puts("PASS: complete-record EOF publication across sectors, ENOSPC, data/inode errors, read-only storage and remount");return 0;
}
