#include "kinfo.h"
#include "v7/fs.h"
#include <nexix/errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned observed;
static v7_ino_t target;
static struct v7fs_inode last;
static int (*original_write)(struct block_device *,uint32_t,const void *);
static int64_t fail_block=-1;
int64_t clock_now(void){return 1709210096;}
static void changed(struct v7fs *fs,const struct v7fs_inode *ip)
{assert(fs->mounted);if(ip->number==target){++observed;last=*ip;}}
static int failing_write(struct block_device *d,uint32_t b,const void *p)
{return b==fail_block?-NX_EIO:original_write(d,b,p);}
static int failing_read(struct block_device *d,uint32_t b,void *p)
{(void)d;(void)b;(void)p;return -NX_EIO;}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long length=ftell(f);assert(length>0 && !fseek(f,0,SEEK_SET));
    unsigned char *bytes=malloc(length);assert(bytes && fread(bytes,1,length,f)==(size_t)length && !fclose(f));
    struct ramdisk ram;assert(!ramdisk_init(&ram,bytes,length,false));
    struct v7fs fs={0};assert(!v7fs_mount(&fs,&ram.device,false));fs.inode_changed=changed;
    char name[V7_DIRSIZ]={0};memcpy(name,"kinfo",5);
    assert(!v7fs_create(&fs,2,name,V7_IFREG|0600,0,0,&target));
    observed=0;size_t done;
    struct nx_kinfo_io before={0},after={0};block_statistics(&before);
    assert(!v7fs_write(&fs,target,0,"123456",6,&done) && done==6);
    assert(observed && last.number==target && last.size==6 && last.nlink==1);
    block_statistics(&after);assert(after.write_sectors>before.write_sectors);
    cache_statistics(&before);struct v7fs_inode inode;
    assert(!v7fs_inode_get(&fs,target,&inode) && !v7fs_inode_get(&fs,target,&inode));
    cache_statistics(&after);assert(after.cache_lookups>=before.cache_lookups+2 && after.cache_hits>=before.cache_hits+2);
    inode.mode=V7_IFREG|0640;inode.uid=123;
    assert(!v7fs_metadata(&fs,&inode) && last.mode==inode.mode && last.uid==123);
    observed=0;original_write=ram.device.write;ram.device.write=failing_write;fail_block=v7_itod(target);
    inode.uid=456;block_statistics(&before);
    assert(v7fs_metadata(&fs,&inode)==-NX_EIO && !observed && last.uid==123 && fs.failed);
    block_statistics(&after);assert(after.write_errors==before.write_errors+1);
    struct block_device bad={.blocks=8,.read=failing_read};unsigned char sector[512];
    block_statistics(&before);assert(block_read(&bad,0,1,sector)==-NX_EIO);block_statistics(&after);
    assert(after.read_errors==before.read_errors+1 && after.read_sectors==before.read_sectors);
    free(bytes);puts("PASS: live metadata publication, failed-write isolation, cache and block statistics");return 0;
}
